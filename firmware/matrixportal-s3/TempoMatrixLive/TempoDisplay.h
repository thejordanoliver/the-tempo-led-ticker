#pragma once
#include "TempoMiniFont.h"
#include "TempoLogos.h"

// This header is included by TempoMatrixLive.ino *after* Game and globals.
// Intentional separate renderer so network behavior and Phase 1 remain intact.
enum class TempoDisplayMode : uint8_t { Scoreboard, Ticker, Logo };
TempoDisplayMode tempoMode = TempoDisplayMode::Scoreboard;
uint32_t lastVisualFrameAt = 0;
int16_t tickerX = 128;
bool showAwayLogo = true;

static const char *tempoLogoLeague() {
  switch (league) {
    case League::NBA: return "nba";
    case League::NFL: return "nfl";
    case League::CFB: return "cfb";
    default: return "unknown";
  }
}

static const char *tempoModeName() {
  switch (tempoMode) {
    case TempoDisplayMode::Scoreboard: return "scoreboard";
    case TempoDisplayMode::Ticker: return "ticker";
    case TempoDisplayMode::Logo: return "logo";
  }
  return "unknown";
}

static void selectTempoMode(TempoDisplayMode mode) {
  tempoMode = mode;
  tickerX = WIDTH;
  showAwayLogo = true;
  lastVisualFrameAt = millis();
  lastRotateAt = millis();
  Serial.print("[tempo] mode: ");
  Serial.println(tempoModeName());
}

static void nextTempoMode() {
  selectTempoMode(static_cast<TempoDisplayMode>(
    (static_cast<uint8_t>(tempoMode) + 1) % 3));
}

static const char *scoreLabel(int16_t score, char (&buffer)[6]) {
  if (score < 0) return "-";
  snprintf(buffer, sizeof(buffer), "%d", score);
  return buffer;
}

static void rankLabel(int8_t rank, char (&buffer)[5]) {
  if (rank <= 0) buffer[0] = '\0';
  else snprintf(buffer, sizeof(buffer), "#%d", rank);
}

static void drawScoreboard(const Game &g) {
  const uint16_t hint = rgb(185, 207, 255);
  const uint16_t white = rgb(245, 247, 255);
  tempoLogo(tempoLogoLeague(), g.away, g.awayColor, 2, 3, 32);
  tempoLogo(tempoLogoLeague(), g.home, g.homeColor, 94, 3, 32);

  // Same 128x64 anchors and variable text scale as simulator/index.html.
  const uint8_t awayScale = tempoTextWidth(g.away, 2) <= 36 ? 2 : 1;
  const uint8_t homeScale = tempoTextWidth(g.home, 2) <= 36 ? 2 : 1;
  tempoText(18 - tempoTextWidth(g.away, awayScale) / 2, 38, g.away, g.awayColor, awayScale);
  tempoText(110 - tempoTextWidth(g.home, homeScale) / 2, 38, g.home, g.homeColor, homeScale);
  char rank[5];
  rankLabel(g.awayRank, rank);
  if (*rank) tempoText(38, 38 + 5 * (awayScale - 1), rank, hint);
  rankLabel(g.homeRank, rank);
  if (*rank) tempoText(90 - tempoTextWidth(rank), 38 + 5 * (homeScale - 1), rank, hint);

  tempoText(18 - tempoTextWidth(g.scheduled ? (*g.awayRecord ? g.awayRecord : "-") : "AWAY") / 2,
            50, g.scheduled ? (*g.awayRecord ? g.awayRecord : "-") : "AWAY", hint);
  tempoText(110 - tempoTextWidth(g.scheduled ? (*g.homeRecord ? g.homeRecord : "-") : "HOME") / 2,
            50, g.scheduled ? (*g.homeRecord ? g.homeRecord : "-") : "HOME", hint);

  char aw[6], hm[6], matchup[16];
  if (g.scheduled) strcpy(matchup, "VS");
  else snprintf(matchup, sizeof(matchup), "%s-%s",
                scoreLabel(g.awayScore, aw), scoreLabel(g.homeScore, hm));
  const uint8_t scoreScale = tempoTextWidth(matchup, 2) <= 56 ? 2 : 1;
  tempoCentered(matchup, 16, white, scoreScale);
  tempoCentered(g.status, 58, hint);
}

static void tickerSegment(const Game &g, char (&line)[128]) {
  char aw[6], hm[6], awRank[5], hmRank[5];
  rankLabel(g.awayRank, awRank);
  rankLabel(g.homeRank, hmRank);
  char matchup[16];
  if (g.scheduled) strcpy(matchup, "VS");
  else snprintf(matchup, sizeof(matchup), "%s-%s",
                scoreLabel(g.awayScore, aw), scoreLabel(g.homeScore, hm));
  snprintf(line, sizeof(line), "%s%s%s %s %s%s%s %s   /   ",
           awRank, *awRank ? " " : "", g.away, matchup,
           hmRank, *hmRank ? " " : "", g.home, g.status);
}

static void drawTicker(const Game &g) {
  const uint16_t hint = rgb(185, 207, 255);
  tempoCentered("TEMPO", 5, rgb(115, 176, 255), 2);
  char line[128];
  tickerSegment(g, line);
  tempoText(tickerX, 32, line, rgb(240, 245, 255), 2);
  if (stale) tempoText(94, 1, "STALE", rgb(240, 135, 60));
}

static void drawLogoMode(const Game &g) {
  const char *team = showAwayLogo ? g.away : g.home;
  const uint16_t color = showAwayLogo ? g.awayColor : g.homeColor;
  const int8_t rank = showAwayLogo ? g.awayRank : g.homeRank;
  tempoLogo(tempoLogoLeague(), team, color, 40, 1, 48);
  const uint8_t scale = tempoTextWidth(team, 2) <= WIDTH ? 2 : 1;
  tempoCentered(team, 53, rgb(245, 247, 255), scale);
  if (rank > 0) {
    char label[5];
    rankLabel(rank, label);
    tempoText(3, 57, label, rgb(185, 207, 255));
  }
  if (stale) tempoText(99, 57, "OLD", rgb(240, 135, 60));
}

void draw() {
  matrix.fillScreen(rgb(3, 5, 10));
  const uint16_t hint = rgb(185, 207, 255);
  if (!configured) {
    tempoCentered("SET CONFIG", 28, hint, 2);
  } else if (!hasGoodResponse) {
    tempoCentered(WiFi.status() == WL_CONNECTED ? "NO API DATA" : "CONNECTING",
                  28, hint, 1);
  } else if (gameCount == 0) {
    tempoCentered("NO GAMES", 28, hint, 2);
    if (stale) tempoCentered("STALE", 48, rgb(240, 135, 60));
  } else {
    const Game &g = games[gameIndex];
    if (tempoMode == TempoDisplayMode::Scoreboard) drawScoreboard(g);
    else if (tempoMode == TempoDisplayMode::Ticker) drawTicker(g);
    else drawLogoMode(g);
    if (stale && tempoMode == TempoDisplayMode::Scoreboard) {
      tempoText(43, 0, "STALE", rgb(240, 135, 60));
    }
  }
  matrix.show();
}

// Call frequently from loop; non-blocking animation is independent of Wi-Fi
// except during Phase 2's synchronous HTTP request.
void updateTempoVisuals(uint32_t now) {
  if (gameCount == 0) return;
  if (tempoMode == TempoDisplayMode::Ticker) {
    if (!elapsed(now, lastVisualFrameAt, 55)) return;
    lastVisualFrameAt = now;
    --tickerX;
    char segment[128];
    tickerSegment(games[gameIndex], segment);
    if (static_cast<int32_t>(tickerX) + tempoTextWidth(segment, 2) < 0) {
      tickerX = WIDTH;
      gameIndex = (gameIndex + 1) % gameCount;
    }
    draw();
  } else if (tempoMode == TempoDisplayMode::Logo) {
    if (gameCount > 1 && elapsed(now, lastRotateAt, ROTATE_MS)) {
      gameIndex = (gameIndex + 1) % gameCount;
      lastRotateAt = now;
      showAwayLogo = true;
      lastVisualFrameAt = now;
      draw();
    } else if (elapsed(now, lastVisualFrameAt, 1800)) {
      lastVisualFrameAt = now;
      showAwayLogo = !showAwayLogo;
      draw();
    }
  } else if (gameCount > 1 && elapsed(now, lastRotateAt, ROTATE_MS)) {
    gameIndex = (gameIndex + 1) % gameCount;
    lastRotateAt = now;
    draw();
  }
}
