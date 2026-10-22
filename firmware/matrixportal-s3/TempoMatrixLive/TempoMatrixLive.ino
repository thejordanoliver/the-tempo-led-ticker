#include <Arduino.h>
#include <Adafruit_Protomatter.h>
#include <ArduinoJson.h>          // ArduinoJson 7.x
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ctype.h>
#include <stdlib.h>
#include <time.h>

// Phase 2 is separate from the untouched Phase 1 hardware diagnostic sketch.
#if !defined(ARDUINO_ADAFRUIT_MATRIXPORTAL_ESP32S3)
#error "Select Adafruit MatrixPortal ESP32-S3 in Tools > Board."
#endif

#if __has_include("secrets.h")
#include "secrets.h"
#else
#include "secrets.example.h"     // Compiles safely without real credentials.
#endif

constexpr uint16_t WIDTH = 128;
constexpr uint16_t HEIGHT = 64;
constexpr uint8_t MAX_GAMES = 24;
constexpr uint32_t ROTATE_MS = 5000;
constexpr uint32_t REFRESH_MS = 60000;
constexpr uint32_t RETRY_MS = 15000;
constexpr uint32_t WIFI_RETRY_MS = 15000;
constexpr int32_t MAX_RESPONSE_BYTES = 384 * 1024;
constexpr char NEW_YORK_TZ[] = "EST5EDT,M3.2.0/2,M11.1.0/2";

uint8_t rgbPins[] = {42, 41, 40, 38, 39, 37};
uint8_t addressPins[] = {45, 36, 48, 35, 21};
Adafruit_Protomatter matrix(WIDTH, 4, 1, rgbPins, 5, addressPins, 2, 47, 14, true);

enum class League : uint8_t { NBA, NFL, CFB, Invalid };
enum class DisplayState : uint8_t { Setup, Connecting, Offline, Empty, Games };

struct Game {
  char away[6] = "AWY";
  char home[6] = "HME";
  char status[20] = "SCHEDULED";
  int16_t awayScore = -1;        // -1 means missing; zero is a real score.
  int16_t homeScore = -1;
  uint16_t awayColor = 0;
  uint16_t homeColor = 0;
  bool showScores = false;
  bool scheduled = true;
  int8_t awayRank = -1;
  int8_t homeRank = -1;
  char awayRecord[8] = "";
  char homeRecord[8] = "";
};

Game games[MAX_GAMES];
uint8_t gameCount = 0;
uint8_t gameIndex = 0;
bool hasGoodResponse = false;
bool stale = false;
bool wifiWasConnected = false;
bool fetchImmediately = true;
uint32_t lastRotateAt = 0;
uint32_t lastRequestAt = 0;
uint32_t lastWifiAttemptAt = 0;
uint32_t retryInterval = REFRESH_MS;
bool hasRequested = false;
League league = League::Invalid;
bool configured = false;

bool elapsed(uint32_t now, uint32_t since, uint32_t interval) {
  return static_cast<uint32_t>(now - since) >= interval;
}

const char *leagueName(League selected) {
  switch (selected) {
    case League::NBA: return "NBA";
    case League::NFL: return "NFL";
    case League::CFB: return "CFB";
    default: return "ERR";
  }
}

League chooseLeague() {
  if (strcmp(TEMPO_LEAGUE, "nba") == 0) return League::NBA;
  if (strcmp(TEMPO_LEAGUE, "nfl") == 0) return League::NFL;
  if (strcmp(TEMPO_LEAGUE, "cfb") == 0) return League::CFB;
  return League::Invalid;
}

uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return matrix.color565(r, g, b);
}

uint16_t readColor(const char *hex) {
  if (!hex) return rgb(170, 187, 221);
  if (*hex == '#') ++hex;
  if (strlen(hex) != 6) return rgb(170, 187, 221);
  for (uint8_t i = 0; i < 6; ++i) {
    if (!isxdigit(static_cast<unsigned char>(hex[i]))) return rgb(170, 187, 221);
  }
  const unsigned long raw = strtoul(hex, nullptr, 16);
  uint8_t r = (raw >> 16) & 255;
  uint8_t g = (raw >> 8) & 255;
  uint8_t b = raw & 255;
  if (r < 190 && g < 190 && b < 190) {
    r = r + ((255 - r) * 45) / 100;
    g = g + ((255 - g) * 45) / 100;
    b = b + ((255 - b) * 45) / 100;
  }
  return rgb(r, g, b);
}

void teamCode(const char *input, char (&output)[6], const char *fallback) {
  uint8_t n = 0;
  if (input) {
    for (const char *p = input; *p && n < 5; ++p) {
      unsigned char ch = static_cast<unsigned char>(*p);
      if (ch >= 'a' && ch <= 'z') ch -= 32;
      if ((ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9')) {
        output[n++] = static_cast<char>(ch);
      }
    }
  }
  if (n == 0) {
    strncpy(output, fallback, 5);
    output[5] = '\0';
  } else {
    output[n] = '\0';
  }
}

int16_t readScore(JsonVariantConst value) {
  if (value.is<bool>() || value.isNull()) return -1;
  if (value.is<int>()) {
    const int score = value.as<int>();
    return score >= 0 && score <= 999 ? score : -1;
  }
  const char *text = value.as<const char *>();
  if (!text || !*text) return -1;
  char *end = nullptr;
  long number = strtol(text, &end, 10);
  return (end != text && *end == '\0' && number >= 0 && number <= 999)
             ? static_cast<int16_t>(number) : -1;
}


int8_t readRank(JsonVariantConst value) {
  if (value.is<bool>() || value.isNull()) return -1;
  int n = -1;
  if (value.is<int>()) n = value.as<int>();
  else {
    const char *text = value.as<const char *>();
    if (!text || !*text) return -1;
    char *end = nullptr;
    const long v = strtol(text, &end, 10);
    if (end == text || *end != '\0' || v < 1 || v > 98) return -1;
    n = static_cast<int>(v);
  }
  return n > 0 && n < 99 ? static_cast<int8_t>(n) : -1;
}

void readRecord(JsonVariantConst value, char (&output)[8]) {
  output[0] = '\0';
  const char *text = value.as<const char *>();
  if (!text || strlen(text) >= sizeof(output)) return;
  const char *dash = strchr(text, '-');
  if (!dash || dash == text || dash[1] == '\0' || strchr(dash + 1, '-')) return;
  for (const char *p = text; *p; ++p) {
    if (p != dash && !isdigit(static_cast<unsigned char>(*p))) return;
  }
  strcpy(output, text);
}

// Parse ISO8601 timestamps, including Z, fractional seconds and +/-HH:MM.
// tzset() in setup configures America/New_York; timegm treats input as UTC.
bool formatLocalStart(const char *raw, char (&output)[20]) {
  if (!raw) return false;
  int year=0, month=0, day=0, hour=0, minute=0, second=0, count=0;
  if (sscanf(raw, "%4d-%2d-%2dT%2d:%2d:%2d%n",
             &year, &month, &day, &hour, &minute, &second, &count) != 6)
    return false;
  if (year < 2020 || year > 2100 || month < 1 || month > 12 ||
      day < 1 || day > 31 || hour > 23 || minute > 59 || second > 59)
    return false;
  const char *zone = raw + count;
  if (*zone == '.') {
    ++zone;
    if (!isdigit(static_cast<unsigned char>(*zone))) return false;
    while (isdigit(static_cast<unsigned char>(*zone))) ++zone;
  }
  int offsetSeconds = 0;
  if (*zone == 'Z' && zone[1] == '\0') {
    // Already UTC.
  } else if ((*zone == '+' || *zone == '-') && strlen(zone) == 6 &&
             zone[3] == ':' && isdigit(static_cast<unsigned char>(zone[1])) &&
             isdigit(static_cast<unsigned char>(zone[2])) &&
             isdigit(static_cast<unsigned char>(zone[4])) &&
             isdigit(static_cast<unsigned char>(zone[5]))) {
    const int oh = (zone[1] - '0') * 10 + (zone[2] - '0');
    const int om = (zone[4] - '0') * 10 + (zone[5] - '0');
    if (oh > 23 || om > 59) return false;
    offsetSeconds = (oh * 3600 + om * 60) * (*zone == '+' ? 1 : -1);
  } else return false;
  tm utc = {};
  utc.tm_year = year - 1900;
  utc.tm_mon = month - 1;
  utc.tm_mday = day;
  utc.tm_hour = hour;
  utc.tm_min = minute;
  utc.tm_sec = second;
  const time_t timestamp = timegm(&utc);
  if (timestamp < 0) return false;
  tm check = {};
  gmtime_r(&timestamp, &check);
  if (check.tm_year != year - 1900 || check.tm_mon != month - 1 ||
      check.tm_mday != day || check.tm_hour != hour ||
      check.tm_min != minute || check.tm_sec != second) return false;
  const time_t adjusted = timestamp - offsetSeconds;
  tm local = {};
  if (!localtime_r(&adjusted, &local)) return false;
  const int h12 = local.tm_hour % 12 ? local.tm_hour % 12 : 12;
  snprintf(output, 20, "%d:%02d %s", h12, local.tm_min,
           local.tm_hour < 12 ? "AM" : "PM");
  return true;
}

void setStatus(JsonVariantConst value, Game &game) {
  JsonObjectConst status = value.as<JsonObjectConst>();
  const char *state = status["state"] | "";
  String detail = String(status["description"] | "") + " " +
                  String(status["shortDetail"] | "");
  detail.toUpperCase();

  const bool final = (status["completed"] | false) ||
                     strcmp(state, "post") == 0 || detail.startsWith("FINAL") ||
                     detail.indexOf(" FINAL") >= 0;
  const bool halftime = detail.indexOf("HALFTIME") >= 0 ||
                        detail.indexOf("HALF TIME") >= 0;
  const bool live = strcmp(state, "in") == 0;
  game.showScores = live || halftime || final;

  if (detail.indexOf("POSTPONED") >= 0) {
    strcpy(game.status, "POSTPONED");
    game.showScores = false;
  } else if (detail.indexOf("CANCEL") >= 0) {
    strcpy(game.status, "CANCELED");
    game.showScores = false;
  } else if (detail.indexOf("SUSPEND") >= 0) {
    strcpy(game.status, "SUSPENDED");
    game.showScores = false;
  } else if (detail.indexOf("DELAY") >= 0) {
    strcpy(game.status, "DELAYED");
    game.showScores = false;
  } else if (final) {
    strcpy(game.status, "FINAL");
  } else if (halftime) {
    strcpy(game.status, "HALFTIME");
  } else if (live) {
    const int period = status["period"] | 0;
    const char *clock = status["displayClock"] | "";
    char periodText[8];
    if (period > 4 && period <= 20) {
      if (period == 5) strcpy(periodText, "OT");
      else snprintf(periodText, sizeof(periodText), "OT%d", period - 4);
    } else if (period > 0 && period <= 4) {
      snprintf(periodText, sizeof(periodText), "Q%d", period);
    } else {
      strcpy(periodText, "LIVE");
    }
    // Only show a bounded digital clock, avoiding overlong text on 128 pixels.
    bool validClock = strlen(clock) <= 6 && strlen(clock) >= 3;
    for (const char *p = clock; *p && validClock; ++p) {
      if (!isdigit(static_cast<unsigned char>(*p)) && *p != ':' && *p != '.') {
        validClock = false;
      }
    }
    if (validClock && strchr(clock, ':')) {
      snprintf(game.status, sizeof(game.status), "%s %s", periodText, clock);
    } else {
      snprintf(game.status, sizeof(game.status), "%s", periodText);
    }
  } else {
    strcpy(game.status, "SCHEDULED");
  }
  game.scheduled = !game.showScores;
}

void setupFilter(JsonDocument &filter) {
  filter["games"][0]["date"] = true;
  filter["games"][0]["away"]["rank"] = true;
  filter["games"][0]["away"]["record"] = true;
  filter["games"][0]["home"]["rank"] = true;
  filter["games"][0]["home"]["record"] = true;
  filter["games"][0]["away"]["code"] = true;
  filter["games"][0]["away"]["score"] = true;
  filter["games"][0]["away"]["primaryColor"] = true;
  filter["games"][0]["home"]["code"] = true;
  filter["games"][0]["home"]["score"] = true;
  filter["games"][0]["home"]["primaryColor"] = true;
  filter["games"][0]["status"]["state"] = true;
  filter["games"][0]["status"]["completed"] = true;
  filter["games"][0]["status"]["description"] = true;
  filter["games"][0]["status"]["shortDetail"] = true;
  filter["games"][0]["status"]["period"] = true;
  filter["games"][0]["status"]["displayClock"] = true;
}

bool parseGames(Stream &stream) {
  JsonDocument filter;
  setupFilter(filter);
  JsonDocument document;
  const DeserializationError error = deserializeJson(
      document, stream, DeserializationOption::Filter(filter),
      DeserializationOption::NestingLimit(12));
  if (error) {
    Serial.print("[tempo] JSON error: ");
    Serial.println(error.c_str());
    return false;
  }

  JsonArrayConst incoming = document["games"].as<JsonArrayConst>();
  if (incoming.isNull()) {
    Serial.println("[tempo] Invalid API envelope: expected games array");
    return false;
  }

  Game proposed[MAX_GAMES];
  uint8_t found = 0;
  for (JsonVariantConst entry : incoming) {
    if (found == MAX_GAMES) break;
    JsonObjectConst item = entry.as<JsonObjectConst>();
    JsonObjectConst away = item["away"].as<JsonObjectConst>();
    JsonObjectConst home = item["home"].as<JsonObjectConst>();
    if (away.isNull() || home.isNull()) continue;

    Game &g = proposed[found];
    teamCode(away["code"] | "", g.away, "AWY");
    teamCode(home["code"] | "", g.home, "HME");
    g.awayScore = readScore(away["score"]);
    g.homeScore = readScore(home["score"]);
    g.awayColor = readColor(away["primaryColor"] | "");
    g.homeColor = readColor(home["primaryColor"] | "");
    setStatus(item["status"], g);
    g.awayRank = readRank(away["rank"]);
    g.homeRank = readRank(home["rank"]);
    readRecord(away["record"], g.awayRecord);
    readRecord(home["record"], g.homeRecord);
    if (strcmp(g.status, "SCHEDULED") == 0) {
      char startTime[20];
      if (formatLocalStart(item["date"] | "", startTime))
        strcpy(g.status, startTime);
    }
    ++found;
  }

  if (incoming.size() > 0 && found == 0) {
    Serial.println("[tempo] Invalid games; preserving cached data");
    return false;
  }
  for (uint8_t i = 0; i < found; ++i) games[i] = proposed[i];
  gameCount = found;
  if (gameCount == 0) gameIndex = 0;
  else gameIndex %= gameCount;
  hasGoodResponse = true;
  stale = false;
  Serial.print("[tempo] parsed games: ");
  Serial.print(gameCount);
  if (incoming.size() > MAX_GAMES) Serial.print(" (capped at 24)");
  Serial.println();
  return true;
}

String buildUrl() {
  String base = TEMPO_API_BASE_URL;
  base.trim();
  while (base.endsWith("/")) base.remove(base.length() - 1);
  switch (league) {
    case League::NBA: {
      String url = base + "/api/games/basketball/nba";
      String date = TEMPO_NBA_DATE;
      if (date.length() == 0) {
        tm local;
        if (getLocalTime(&local, 0) && local.tm_year >= 124) {
          char buffer[9];
          strftime(buffer, sizeof(buffer), "%Y%m%d", &local);
          date = buffer;
        }
      }
      if (date.length() == 8) url += "?date=" + date;
      return url;
    }
    case League::NFL: return base + "/api/games/football/nfl?season=" +
                        String(TEMPO_FOOTBALL_SEASON) + "&seasontype=" +
                        String(TEMPO_FOOTBALL_SEASONTYPE) + "&week=" +
                        String(TEMPO_FOOTBALL_WEEK);
    case League::CFB: return base + "/api/games/football/cfb?season=" +
                        String(TEMPO_FOOTBALL_SEASON) + "&seasontype=" +
                        String(TEMPO_FOOTBALL_SEASONTYPE) + "&week=" +
                        String(TEMPO_FOOTBALL_WEEK);
    default: return "";
  }
}

void drawLabel(int16_t x, int16_t y, const char *text, uint16_t color,
               uint8_t scale = 1) {
  matrix.setTextColor(color);
  matrix.setTextSize(scale);
  matrix.setCursor(x, y);
  matrix.print(text);
  matrix.setTextSize(1);
}

void drawCenter(int16_t y, const char *label, uint16_t color) {
  int16_t x = (WIDTH - static_cast<int16_t>(strlen(label)) * 6) / 2;
  drawLabel(x < 0 ? 0 : x, y, label, color);
}

DisplayState screenState() {
  if (!configured) return DisplayState::Setup;
  if (hasGoodResponse) return gameCount == 0 ? DisplayState::Empty : DisplayState::Games;
  return WiFi.status() == WL_CONNECTED ? DisplayState::Offline : DisplayState::Connecting;
}

void draw() {
  matrix.fillScreen(0);
  matrix.setTextWrap(false);
  const uint16_t muted = rgb(105, 135, 165);
  const uint16_t bright = rgb(205, 215, 225);

  drawLabel(3, 2, "TEMPO", rgb(0, 165, 155));
  drawLabel(stale ? 94 : 108, 2, stale ? "STALE" : leagueName(league),
            stale ? rgb(235, 125, 60) : muted);
  matrix.drawFastHLine(0, 12, WIDTH, muted);
  const DisplayState state = screenState();
  if (state != DisplayState::Games) {
    if (state == DisplayState::Setup) drawCenter(28, "SET CONFIG", bright);
    else if (state == DisplayState::Connecting) drawCenter(28, "CONNECTING", bright);
    else if (state == DisplayState::Offline) drawCenter(28, "NO API DATA", bright);
    else drawCenter(28, "NO GAMES", bright);
  } else {
    const Game &g = games[gameIndex];
    drawLabel(8, 17, g.away, g.awayColor);
    const int16_t homeX = WIDTH - static_cast<int16_t>(strlen(g.home)) * 6 - 8;
    drawLabel(homeX, 17, g.home, g.homeColor);

    char awayScore[5];
    char homeScore[5];
    if (g.showScores && g.awayScore >= 0)
      snprintf(awayScore, sizeof(awayScore), "%d", g.awayScore);
    else strcpy(awayScore, "-");
    if (g.showScores && g.homeScore >= 0)
      snprintf(homeScore, sizeof(homeScore), "%d", g.homeScore);
    else strcpy(homeScore, "-");
    drawLabel(8, 29, awayScore, bright, 2);
    drawLabel(84, 29, homeScore, bright, 2);
    drawLabel(61, 34, "-", muted);
    matrix.drawFastHLine(0, 51, WIDTH, muted);
    drawCenter(55, g.status, rgb(185, 200, 235));
  }
  matrix.show();
}

bool requestWithClient(WiFiClient &client, const String &url) {
  HTTPClient http;
  http.useHTTP10(true);          // Avoid chunked responses when parsing Stream.
  http.setConnectTimeout(5000);
  http.setTimeout(8000);
  http.setReuse(false);
  if (!http.begin(client, url)) {
    Serial.println("[tempo] Invalid URL or connection setup");
    return false;
  }
  http.addHeader("Accept", "application/json");
  const int status = http.GET();
  bool ok = false;
  if (status == HTTP_CODE_OK) {
    const int len = http.getSize();
    if (len < 0 || len > MAX_RESPONSE_BYTES) {
      Serial.println("[tempo] API requires a bounded Content-Length (max 384 KiB)");
    } else {
      ok = parseGames(http.getStream());
    }
  } else {
    Serial.print("[tempo] HTTP failure: ");
    Serial.println(status);
  }
  http.end();
  return ok;
}

bool fetchScores() {
  const String url = buildUrl();
  const bool https = url.startsWith("https://");
  if (!https && !url.startsWith("http://")) {
    Serial.println("[tempo] API URL must begin with http:// or https://");
    return false;
  }
  if (https) {
    if (strlen(TEMPO_ROOT_CA) == 0) {
      Serial.println("[tempo] HTTPS requires TEMPO_ROOT_CA; refusing insecure TLS");
      return false;
    }
    tm utc;
    if (!getLocalTime(&utc, 0) || utc.tm_year < 124) {
      Serial.println("[tempo] Waiting for NTP before TLS verification");
      return false;
    }
    WiFiClientSecure client;
    client.setCACert(TEMPO_ROOT_CA);
    client.setTimeout(8000);
    return requestWithClient(client, url);
  }
  WiFiClient client;
  client.setTimeout(8000);
  return requestWithClient(client, url);
}

void printHelp() {
  Serial.println("[tempo] n next game | r retry fetch | h help");
  Serial.println("[tempo] Refresh: 60s, retry on error: 15s, rotation: 5s");
  Serial.println("[tempo] No credentials/API secrets are printed");
}

void setup() {
  Serial.begin(115200);
  delay(400);
  Serial.println("\n[tempo] Phase 2 Wi-Fi scoreboard (hardware validation pending)");
  const ProtomatterStatus result = matrix.begin();
  Serial.print("[tempo] Protomatter status: ");
  Serial.println(static_cast<int>(result));
  if (result != PROTOMATTER_OK) {
    Serial.println("[tempo] Matrix failed to initialize");
    for (;;) delay(500);
  }
  league = chooseLeague();
  String base = TEMPO_API_BASE_URL;
  const bool urlValid = base.startsWith("http://") || base.startsWith("https://");
  const bool footballValid = TEMPO_FOOTBALL_SEASON >= 2000 &&
                             TEMPO_FOOTBALL_WEEK >= 1 && TEMPO_FOOTBALL_WEEK <= 25 &&
                             TEMPO_FOOTBALL_SEASONTYPE >= 1 &&
                             TEMPO_FOOTBALL_SEASONTYPE <= 3;
  configured = strlen(TEMPO_WIFI_SSID) > 0 && strlen(TEMPO_API_BASE_URL) > 0 &&
               urlValid && league != League::Invalid &&
               (league == League::NBA || footballValid);
  draw();
  printHelp();
  if (!configured) {
    Serial.println("[tempo] Edit secrets.h using secrets.example.h (see README)");
    return;
  }
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(TEMPO_WIFI_SSID, TEMPO_WIFI_PASSWORD);
  lastWifiAttemptAt = millis();
  Serial.println("[tempo] Connecting to Wi-Fi");
}

void loop() {
  const uint32_t now = millis();
  while (Serial.available()) {
    switch (Serial.read()) {
      case 'n': case 'N':
        if (gameCount > 1) {
          gameIndex = (gameIndex + 1) % gameCount;
          lastRotateAt = now;
          draw();
        }
        break;
      case 'r': case 'R': fetchImmediately = true; break;
      case 'h': case 'H': printHelp(); break;
      default: break;
    }
  }
  if (!configured) { delay(30); return; }

  const bool connected = WiFi.status() == WL_CONNECTED;
  if (!connected) {
    if (wifiWasConnected) {
      wifiWasConnected = false;
      stale = hasGoodResponse;
      Serial.println("[tempo] Wi-Fi disconnected; retaining last scores");
      draw();
    }
    if (elapsed(now, lastWifiAttemptAt, WIFI_RETRY_MS)) {
      WiFi.disconnect();
      WiFi.begin(TEMPO_WIFI_SSID, TEMPO_WIFI_PASSWORD);
      lastWifiAttemptAt = now;
      Serial.println("[tempo] Wi-Fi reconnect attempt");
    }
  } else {
    if (!wifiWasConnected) {
      wifiWasConnected = true;
      fetchImmediately = true;
      configTzTime(NEW_YORK_TZ, "pool.ntp.org", "time.nist.gov");
      Serial.print("[tempo] Wi-Fi connected, IP ");
      Serial.println(WiFi.localIP());
      draw();
    }
    if (fetchImmediately ||
        (hasRequested && elapsed(now, lastRequestAt, retryInterval))) {
      fetchImmediately = false;
      hasRequested = true;
      lastRequestAt = millis();
      Serial.print("[tempo] Requesting ");
      Serial.println(leagueName(league));
      const bool success = fetchScores();
      retryInterval = success ? REFRESH_MS : RETRY_MS;
      if (!success) stale = hasGoodResponse;
      if (!success) Serial.println("[tempo] Scores unavailable; retry scheduled");
      draw();
    }
  }

  if (gameCount > 1 && elapsed(millis(), lastRotateAt, ROTATE_MS)) {
    gameIndex = (gameIndex + 1) % gameCount;
    lastRotateAt = millis();
    draw();
  }
  delay(10);
}
