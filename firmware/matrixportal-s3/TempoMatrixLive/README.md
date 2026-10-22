# Phase 2 — Wi-Fi Tempo scoreboard (Matrix Portal S3)

**Status:** source is ready for local Arduino compilation; it has **not** been flashed or verified on real hardware. This is a **separate sketch** from the Phase 1 color/grid diagnostic. Verify Phase 1 first when the hardware arrives.

## What it does

- Renders NBA, NFL or college-football games from the existing **read-only Tempo backend** on one **128×64 HUB75 / 1:32 scan** matrix.
- Connects to Wi-Fi, retries connection every 15 seconds if needed, retrieves game data every **60 seconds**, and retries API failures after **15 seconds**.
- Rotates up to **24 games**, normally every **5 seconds**. The HTTPS/HTTP request runs synchronously with an 8-second read timeout, so the screen can temporarily stop rotating during a slow request.
- Shows scores, team abbreviations, team colors, live period/clock, halftime and final states. Scheduled games show a dash for scores and `SCHEDULED` (display-time conversion and logos are Phase 3).
- Treats score **0** as valid and missing scores as `-`. Shows `NO GAMES` for successful empty schedules and `STALE` on the last successful set of games after API or Wi-Fi failures.
- Uses the existing JSON envelope `{ "games": [ ... ] }`. Incorrect envelopes, failed requests and malformed JSON do not overwrite successfully cached scores.
- Rejects HTTPS requests with missing root-CA certificates rather than bypassing TLS validation.

## Setup without hardware

1. Install **esp32 by Espressif Systems**, select **Adafruit MatrixPortal ESP32-S3** under Arduino IDE → Tools → Board.
2. Install **Adafruit Protomatter**, **Adafruit GFX Library**, and **ArduinoJson 7.x** in Library Manager. HTTPClient, WiFi and WiFiClientSecure are provided by the Espressif ESP32 board package.
3. Open **TempoMatrixLive.ino** and click **Verify**. The sketch intentionally compiles with empty settings from `secrets.example.h`; it would display **SET CONFIG** if flashed without configuration.
4. When ready for network testing, copy `secrets.example.h` to **`secrets.h`** inside this same sketch directory. Fill in your own Wi-Fi SSID/password and the accessible API base URL. **Do not share or commit `secrets.h`**; the root `.gitignore` excludes it.
5. Upload and use Serial Monitor at **115200 baud**. Send `r` to retry the API, `n` to display the next game, or `h` for controls.

## Example local configuration

```cpp
#define TEMPO_WIFI_SSID "MyHomeWiFi"
#define TEMPO_WIFI_PASSWORD "replace-with-your-private-password"
#define TEMPO_API_BASE_URL "http://192.168.1.50:4000"
#define TEMPO_LEAGUE "nba"
#define TEMPO_NBA_DATE ""
#define TEMPO_FOOTBALL_SEASON 2026
#define TEMPO_FOOTBALL_SEASONTYPE 2
#define TEMPO_FOOTBALL_WEEK 1
#define TEMPO_ROOT_CA ""
```

**Important:** the IP address above is an **illustration**, not your actual Mac's IP. While the Tempo backend runs on your Mac, use its **LAN IPv4 address** and make sure it listens on an interface the ESP32 can reach; allow the appropriate local firewall access. **Do not use `localhost` or `127.0.0.1`**: these point at the ESP32 when used by its firmware. The ESP32 does not use browser CORS restrictions.

Do not expose your development server to the public internet. LAN **HTTP is intended only for your trusted private network**. For a deployed HTTPS API, provide the **correct trusted root CA PEM certificate** as `TEMPO_ROOT_CA` in `secrets.h`; the firmware synchronizes time with NTP and validates TLS. It intentionally does not support `setInsecure()`. If you don't have a verified CA setup, use a trusted LAN for early hardware tests and leave HTTPS deployment for a later verification step.

## API paths

| League | Endpoint |
| --- | --- |
| NBA | `/api/games/basketball/nba?date=YYYYMMDD` after NTP sync (America/New_York); otherwise no date so backend uses its default |
| NFL | `/api/games/football/nfl?season=2026&seasontype=2&week=1` |
| CFB | `/api/games/football/cfb?season=2026&seasontype=2&week=1` |

The NBA date may be manually overridden with `TEMPO_NBA_DATE`. **The football week/season are manually configured**; the firmware does *not* automatically determine the current football week. Set a valid target week appropriate to the schedule you want. It does not modify the backend.

Example supported nested response (illustrative; no actual game results):

```json
{
  "games": [
    {
      "date": "2026-10-22T23:00:00Z",
      "status": {
        "state": "in",
        "period": 4,
        "displayClock": "2:35",
        "completed": false
      },
      "away": { "code": "BOS", "score": 110, "primaryColor": "#00834D" },
      "home": { "code": "NYK", "score": 108, "primaryColor": "#F58426" }
    }
  ]
}
```

Filtering retains only the properties needed for the simple physical scoreboard. The firmware streams JSON using **HTTP/1.0** to avoid chunked-transfer overhead; it requires a **Content-Length** between 0 and **384 KiB**, validates the `games` array and limits stored games to 24. If the backend doesn't send a Content-Length (or sends a larger response), the firmware fails safely and retries; a later phase can implement full chunked streaming and pagination.

## Verification (pending hardware)

- [ ] **Verify** succeeds for `TempoMatrixLive.ino` in Arduino IDE with ArduinoJson **7.x**.
- [ ] Phase 1 diagnostic sketch shows correct colors and all 128×64 rows on the physical panel.
- [ ] Serial Monitor shows the configured Wi-Fi connection and successful API fetch; the panel renders live/finished/scheduled scores.
- [ ] The ticker rotates games normally every 5 seconds, and `n` advances the selected game.
- [ ] Turn off backend or Wi-Fi: cached games remain displayed and become marked **STALE**.
- [ ] Restart backend / restore Wi-Fi: scores refresh and **STALE** clears.
- [ ] Empty schedules show **NO GAMES**; malformed/oversized responses do not replace existing data.
- [ ] HTTPS root certificate validation succeeds if using a deployed backend.

**Keep Phase 1**: `../TempoMatrixBringup/TempoMatrixBringup.ino` remains the hardware diagnostic / fallback sketch.

Technical references: [MatrixPortal S3 Protomatter pin example](https://github.com/adafruit/Adafruit_Protomatter/blob/master/examples/simple/simple.ino), [ArduinoJson 7 stream/HTTPClient guidance](https://arduinojson.org/v7/how-to/use-arduinojson-with-httpclient/), [ArduinoJson filtering](https://arduinojson.org/v7/example/filter/).
