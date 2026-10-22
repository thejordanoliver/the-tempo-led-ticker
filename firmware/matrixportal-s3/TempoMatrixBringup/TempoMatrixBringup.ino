#include <Arduino.h>
#include <Adafruit_Protomatter.h>

// Phase 1: local-only diagnostics. No Wi-Fi, API calls, or credentials.
#if !defined(ARDUINO_ADAFRUIT_MATRIXPORTAL_ESP32S3)
#error "Select the Adafruit MatrixPortal ESP32-S3 board in Arduino IDE."
#endif

constexpr uint16_t PANEL_WIDTH = 128;
constexpr uint16_t PANEL_HEIGHT = 64;
constexpr uint8_t RGB_DEPTH = 4;
constexpr uint8_t ADDRESS_LINES = 5; // 1/32 scan: A, B, C, D, E.

// Official MatrixPortal S3 (rev C) / Adafruit Protomatter pin mapping.
uint8_t rgbPins[] = {42, 41, 40, 38, 39, 37};
uint8_t addressPins[] = {45, 36, 48, 35, 21};
constexpr uint8_t CLOCK_PIN = 2;
constexpr uint8_t LATCH_PIN = 47;
constexpr uint8_t OE_PIN = 14;

Adafruit_Protomatter matrix(PANEL_WIDTH, RGB_DEPTH, 1, rgbPins,
                            ADDRESS_LINES, addressPins, CLOCK_PIN,
                            LATCH_PIN, OE_PIN, true);

enum class Screen : uint8_t { Colors = 0, Grid = 1, Scoreboard = 2 };
Screen currentScreen = Screen::Colors;
uint32_t screenStartedAt = 0;

constexpr uint32_t COLOR_DURATION_MS = 4000;
constexpr uint32_t GRID_DURATION_MS = 4000;
constexpr uint32_t SCOREBOARD_DURATION_MS = 8000;

uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return matrix.color565(r, g, b);
}

void resetCanvas() {
  matrix.fillScreen(0);
  matrix.setTextWrap(false);
  matrix.setTextSize(1);
  matrix.setTextColor(rgb(190, 205, 220));
}

void textAt(int16_t x, int16_t y, const char *text, uint16_t color,
            uint8_t size = 1) {
  matrix.setTextSize(size);
  matrix.setTextColor(color);
  matrix.setCursor(x, y);
  matrix.print(text);
  matrix.setTextSize(1);
}

void drawColors() {
  resetCanvas();
  // Deliberately dim fills: don't begin hardware testing with full-white output.
  matrix.fillRect(0, 0, 32, 64, rgb(80, 0, 0));
  matrix.fillRect(32, 0, 32, 64, rgb(0, 80, 0));
  matrix.fillRect(64, 0, 32, 64, rgb(0, 0, 100));
  matrix.fillRect(96, 0, 32, 64, rgb(55, 55, 55));
  const uint16_t label = rgb(225, 225, 225);
  textAt(12, 28, "R", label);
  textAt(44, 28, "G", label);
  textAt(76, 28, "B", label);
  textAt(108, 28, "W", label);
}

void drawGrid() {
  resetCanvas();
  const uint16_t grid = rgb(0, 75, 90);
  const uint16_t edge = rgb(160, 140, 0);
  for (int16_t x = 32; x < PANEL_WIDTH; x += 32) {
    matrix.drawFastVLine(x, 0, PANEL_HEIGHT, grid);
  }
  for (int16_t y = 16; y < PANEL_HEIGHT; y += 16) {
    matrix.drawFastHLine(0, y, PANEL_WIDTH, grid);
  }
  matrix.drawRect(0, 0, PANEL_WIDTH, PANEL_HEIGHT, edge);
  const uint16_t label = rgb(210, 210, 210);
  textAt(3, 3, "0,0", label);
  textAt(90, 3, "127,0", label);
  textAt(43, 28, "128x64", label);
  textAt(3, 55, "0,63", label);
  textAt(87, 55, "127,63", label);
}

void drawScoreboard() {
  resetCanvas();
  const uint16_t muted = rgb(100, 130, 160);
  const uint16_t bright = rgb(205, 215, 225);
  textAt(3, 2, "TEMPO", rgb(0, 175, 165));
  textAt(100, 2, "DEMO", muted);
  matrix.drawFastHLine(0, 12, PANEL_WIDTH, muted);

  textAt(10, 17, "BOS", rgb(65, 190, 125));
  textAt(100, 17, "NYK", rgb(235, 140, 60));
  textAt(8, 29, "110", bright, 2);
  textAt(84, 29, "108", bright, 2);
  textAt(61, 34, "-", muted);

  matrix.drawFastHLine(0, 51, PANEL_WIDTH, muted);
  textAt(49, 55, "FINAL", rgb(200, 195, 75));
}

uint32_t screenDuration(Screen screen) {
  switch (screen) {
    case Screen::Colors: return COLOR_DURATION_MS;
    case Screen::Grid: return GRID_DURATION_MS;
    case Screen::Scoreboard: return SCOREBOARD_DURATION_MS;
  }
  return COLOR_DURATION_MS;
}

const char *screenName(Screen screen) {
  switch (screen) {
    case Screen::Colors: return "colors";
    case Screen::Grid: return "grid";
    case Screen::Scoreboard: return "sample scoreboard";
  }
  return "unknown";
}

void showScreen(Screen screen) {
  currentScreen = screen;
  switch (screen) {
    case Screen::Colors: drawColors(); break;
    case Screen::Grid: drawGrid(); break;
    case Screen::Scoreboard: drawScoreboard(); break;
  }
  matrix.show();
  screenStartedAt = millis();
  Serial.print("[tempo] displaying ");
  Serial.println(screenName(screen));
}

void nextScreen() {
  showScreen(static_cast<Screen>((static_cast<uint8_t>(currentScreen) + 1) % 3));
}

void printHelp() {
  Serial.println("[tempo] keys: 1 colors | 2 grid | 3 scoreboard | n next | h help");
  Serial.println("[tempo] screens also rotate automatically (4s / 4s / 8s)");
}

void setup() {
  Serial.begin(115200);
  delay(400); // Never wait indefinitely for an open Serial Monitor.
  Serial.println("\n[tempo] Phase 1 MatrixPortal S3 bring-up");
  Serial.println("[tempo] panel=128x64, scan=1/32, address=ABCDE, depth=4");
  Serial.println("[tempo] NO Wi-Fi or live scores in this build");

  const ProtomatterStatus result = matrix.begin();
  Serial.print("[tempo] Protomatter begin() status: ");
  Serial.println(static_cast<int>(result));
  if (result != PROTOMATTER_OK) {
    Serial.println("[tempo] ERROR: matrix initialization failed; check board/pins/power.");
#ifdef LED_BUILTIN
    pinMode(LED_BUILTIN, OUTPUT);
#endif
    for (;;) {
#ifdef LED_BUILTIN
      digitalWrite(LED_BUILTIN, HIGH);
#endif
      delay(250);
#ifdef LED_BUILTIN
      digitalWrite(LED_BUILTIN, LOW);
#endif
      delay(250);
    }
  }

  Serial.println("[tempo] initialization OK");
  printHelp();
  showScreen(Screen::Colors);
}

void loop() {
  while (Serial.available() > 0) {
    const char command = static_cast<char>(Serial.read());
    switch (command) {
      case '1': showScreen(Screen::Colors); break;
      case '2': showScreen(Screen::Grid); break;
      case '3': showScreen(Screen::Scoreboard); break;
      case 'n': case 'N': nextScreen(); break;
      case 'h': case 'H': printHelp(); break;
      default: break; // Ignore newline/other terminal input.
    }
  }
  if (static_cast<uint32_t>(millis() - screenStartedAt) >= screenDuration(currentScreen)) {
    nextScreen();
  }
  delay(5);
}
