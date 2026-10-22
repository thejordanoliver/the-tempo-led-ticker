#pragma once
#include <Arduino.h>
#include <Adafruit_Protomatter.h>
#include <pgmspace.h>
#include "TempoMiniFont.h"

// Generated separately with tools/generate_matrix_logos.py, then compiled
// into ESP32 flash. No remote image decoding or untrusted logo downloads on MCU.
struct TempoLogoEntry {
  const char *league;
  const char *team;
  const uint16_t *rgb565;       // fixed 32x32 RGB565, row-major
  const uint8_t *opaqueMask;    // 1024 bits, MSB first, row-major
};
#if __has_include("logos.generated.h")
#include "logos.generated.h"
#define TEMPO_HAS_LOGOS 1
#else
#define TEMPO_HAS_LOGOS 0
#endif

static const TempoLogoEntry *findTempoLogo(const char *league, const char *team) {
#if TEMPO_HAS_LOGOS
  for (size_t i = 0; i < kTempoLogoCount; ++i) {
    if (strcmp(kTempoLogos[i].league, league) == 0 &&
        strcmp(kTempoLogos[i].team, team) == 0) return &kTempoLogos[i];
  }
#endif
  (void)league;
  (void)team;
  return nullptr;
}

static void tempoLogo(const char *league, const char *team, uint16_t teamColor,
                      int16_t x, int16_t y, uint8_t size) {
  matrix.fillRect(x, y, size, size, matrix.color565(15, 24, 37));
  const TempoLogoEntry *sprite = findTempoLogo(league, team);
  if (sprite) {
    // Nearest-neighbor scale from 32x32 to simulator 32x32 or 48x48 mode.
    // Alpha is a separate bit mask, so even true white remains opaque.
    for (uint8_t dy = 0; dy < size; ++dy) {
      const uint8_t sy = static_cast<uint16_t>(dy) * 32 / size;
      for (uint8_t dx = 0; dx < size; ++dx) {
        const uint8_t sx = static_cast<uint16_t>(dx) * 32 / size;
        const uint16_t index = sy * 32 + sx;
        const uint8_t bits = pgm_read_byte(sprite->opaqueMask + index / 8);
        if (bits & (0x80u >> (index % 8))) {
          matrix.drawPixel(x + dx, y + dy, pgm_read_word(sprite->rgb565 + index));
        }
      }
    }
  } else {
    // Exactly like the browser's blocked/missing-logo fallback.
    char initial[2] = {team && *team ? *team : '?', '\0'};
    const uint8_t scale = size == 48 ? 5 : 3;
    tempoText(x + (size - tempoTextWidth(initial, scale)) / 2,
              y + (size - 5 * scale) / 2, initial, teamColor, scale);
  }
}
