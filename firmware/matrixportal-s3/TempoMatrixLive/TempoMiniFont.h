#pragma once
#include <Arduino.h>
#include <Adafruit_Protomatter.h>
#include <ctype.h>
#include <string.h>

// Port of simulator/index.html miniFont, 3x5 glyphs (except punctuation/#).
// Keep this renderer deliberately tiny rather than using the larger GFX font.
struct TempoGlyph { uint8_t width; uint8_t rows[5]; };
static TempoGlyph tempoGlyph(char symbol) {
  const char ch = static_cast<char>(toupper(static_cast<unsigned char>(symbol)));
  switch (ch) {
    case '#': return {5, {0b1010, 0b11111, 0b1010, 0b11111, 0b1010}};
    case 'A': return {3, {0b10, 0b101, 0b111, 0b101, 0b101}};
    case 'B': return {3, {0b110, 0b101, 0b110, 0b101, 0b110}};
    case 'C': return {3, {0b11, 0b100, 0b100, 0b100, 0b11}};
    case 'D': return {3, {0b110, 0b101, 0b101, 0b101, 0b110}};
    case 'E': return {3, {0b111, 0b100, 0b110, 0b100, 0b111}};
    case 'F': return {3, {0b111, 0b100, 0b110, 0b100, 0b100}};
    case 'G': return {3, {0b11, 0b100, 0b101, 0b101, 0b11}};
    case 'H': return {3, {0b101, 0b101, 0b111, 0b101, 0b101}};
    case 'I': return {3, {0b111, 0b10, 0b10, 0b10, 0b111}};
    case 'J': return {3, {0b1, 0b1, 0b1, 0b101, 0b10}};
    case 'K': return {3, {0b101, 0b101, 0b110, 0b101, 0b101}};
    case 'L': return {3, {0b100, 0b100, 0b100, 0b100, 0b111}};
    case 'M': return {3, {0b101, 0b111, 0b111, 0b101, 0b101}};
    case 'N': return {3, {0b101, 0b111, 0b111, 0b111, 0b101}};
    case 'O': return {3, {0b10, 0b101, 0b101, 0b101, 0b10}};
    case 'P': return {3, {0b110, 0b101, 0b110, 0b100, 0b100}};
    case 'Q': return {3, {0b10, 0b101, 0b101, 0b111, 0b11}};
    case 'R': return {3, {0b110, 0b101, 0b110, 0b101, 0b101}};
    case 'S': return {3, {0b11, 0b100, 0b10, 0b1, 0b110}};
    case 'T': return {3, {0b111, 0b10, 0b10, 0b10, 0b10}};
    case 'U': return {3, {0b101, 0b101, 0b101, 0b101, 0b111}};
    case 'V': return {3, {0b101, 0b101, 0b101, 0b101, 0b10}};
    case 'W': return {3, {0b101, 0b101, 0b111, 0b111, 0b101}};
    case 'X': return {3, {0b101, 0b101, 0b10, 0b101, 0b101}};
    case 'Y': return {3, {0b101, 0b101, 0b10, 0b10, 0b10}};
    case 'Z': return {3, {0b111, 0b1, 0b10, 0b100, 0b111}};
    case '0': return {3, {0b111, 0b101, 0b101, 0b101, 0b111}};
    case '1': return {3, {0b10, 0b110, 0b10, 0b10, 0b111}};
    case '2': return {3, {0b110, 0b1, 0b10, 0b100, 0b111}};
    case '3': return {3, {0b110, 0b1, 0b10, 0b1, 0b110}};
    case '4': return {3, {0b101, 0b101, 0b111, 0b1, 0b1}};
    case '5': return {3, {0b111, 0b100, 0b110, 0b1, 0b110}};
    case '6': return {3, {0b11, 0b100, 0b110, 0b101, 0b10}};
    case '7': return {3, {0b111, 0b1, 0b10, 0b10, 0b10}};
    case '8': return {3, {0b10, 0b101, 0b10, 0b101, 0b10}};
    case '9': return {3, {0b10, 0b101, 0b11, 0b1, 0b110}};
    case ':': return {1, {0b0, 0b1, 0b0, 0b1, 0b0}};
    case '.': return {1, {0b0, 0b0, 0b0, 0b0, 0b1}};
    case '-': return {1, {0b0, 0b0, 0b111, 0b0, 0b0}};
    case '/': return {3, {0b1, 0b1, 0b10, 0b100, 0b100}};
    case ' ': return {1, {0b0, 0b0, 0b0, 0b0, 0b0}};
    default: return {1, {0, 0, 0, 0, 0}};
  }
}

static int16_t tempoTextWidth(const char *message, uint8_t scale = 1) {
  if (!message) return 0;
  int32_t w = 0;
  for (const char *p = message; *p; ++p) w += (tempoGlyph(*p).width + 1) * scale;
  return static_cast<int16_t>(w > INT16_MAX ? INT16_MAX : w);
}

static void tempoText(int16_t x, int16_t y, const char *message,
                      uint16_t color, uint8_t scale = 1) {
  if (!message) return;
  for (const char *p = message; *p; ++p) {
    const TempoGlyph glyph = tempoGlyph(*p);
    if (x + static_cast<int16_t>(glyph.width * scale) >= 0 && x < 128) {
      for (uint8_t row = 0; row < 5; ++row) {
        for (uint8_t col = 0; col < glyph.width; ++col) {
          if (glyph.rows[row] & (1u << (glyph.width - col - 1))) {
            matrix.fillRect(x + col * scale, y + row * scale, scale, scale, color);
          }
        }
      }
    }
    x += (glyph.width + 1) * scale;
    if (x >= 128) break; // No glyph to the right can be visible.
  }
}

static void tempoCentered(const char *label, int16_t y, uint16_t color, uint8_t scale = 1) {
  if (!label) return;
  char bounded[48];
  strlcpy(bounded, label, sizeof(bounded));
  while (*bounded && tempoTextWidth(bounded, scale) > 128) {
    bounded[strlen(bounded) - 1] = '\0';
  }
  tempoText((128 - tempoTextWidth(bounded, scale)) / 2, y, bounded, color, scale);
}
