# Tempo LED Ticker

Browser simulator for the MatrixPortal S3 and a 128×64 HUB75 panel, plus a first-stage **Arduino hardware diagnostic sketch**. The simulator's API access is read only; no browser secrets are required. Phase 1 firmware uses only local test patterns, not the Tempo API.

## Run locally

Start your existing configured backend in a separate terminal:

```sh
cd ~/Projects/the-tempo-backend
npm start
```

Serve the **ticker repository root**, so browser modules are accessible:

```sh
cd ~/Projects/the-tempo-led-ticker
python3 -m http.server 8080 --bind 127.0.0.1
```

Open [the simulator](http://localhost:8080/simulator/). It starts with clearly labeled illustrative demo data and automatically requests the API. Enter a base URL (default `http://localhost:4000`) and click Fetch scores to apply it. Choose NBA, NFL, or College football in the League selector. NBA uses a schedule date (`?date=YYYYMMDD`). NFL and college football use season year, season type, and week (`?season=2026&seasontype=2&week=7`). Football starts at week 1; choose the week you want. Leave the season year blank for the backend’s current season. Changing these controls refreshes automatically. Pasting a weekly endpoint URL populates the football controls. The client requests `/api/games/basketball/nba`, `/api/games/football/nfl`, or `/api/games/football/cfb` and requires a top-level `games` array. Manual JSON accepts that envelope or an array of nested Tempo games or legacy flat demo fields.

Refresh defaults to 60 seconds (minimum 5). Rotation defaults to 5 seconds; use the slider, pause, previous and next controls. Manual refresh applies URL/date settings immediately. Restore demo or Apply JSON stops API polling. Settings last for the current page session.

Timezone defaults to `America/New_York`. The optional schedule date uses `YYYYMMDD`, e.g. `20261016`, and sends `?date=20261016` explicitly. An empty date keeps backend schedule selection unchanged, which may use UTC. Display timezone never changes the requested schedule date.

Scheduled games show team codes, win-loss records and local start time without scores. Missing or invalid records show a dash. Teams appear side by side, away on the left and home on the right, with a centered score and status below. Live games show scores and quarter/clock; halftime and finals have explicit labels. Missing scores show a dash, zero stays zero. Empty schedules show NO GAMES. Network errors retain the previous source, label stale API data and retry automatically; requests time out after 15 seconds. Demo scores are illustrative. IDs, original start times, colors and logo URLs are retained. Codes are sanitized and limited to five characters; dark team colors are lifted for readability. Scores over 999 or invalid scores show a dash to keep the panel layout bounded.

## Verification and troubleshooting

```sh
node --test tests/*.test.mjs
node --check src/api/tempoApi.mjs
node --check src/api/scoreboard.mjs
curl 'http://localhost:4000/api/games/basketball/nba?date=20261016'
```

Unit tests need Node 18+ and no backend. Repeatable Chromium tests and 128×64 image baselines are available with `npm run test:browser`; see [tests/README.md](tests/README.md) for one-time setup and baseline review. Browser testing uses development-only Playwright and requires a Node version supported by the installed Playwright release. For a live smoke test, click Fetch scores and check for API, game count and update time. Change the date to test an empty schedule, stop the API to test stale-data recovery, then restart it. Verify manual rotation and timezone changes.

Use HTTP rather than opening `file://`: ES modules and fetch are restricted for local files. The API must allow the simulator origin `http://localhost:8080` through its existing CORS configuration. Curl success alone does not prove browser CORS works. Do not disable browser security. Check the browser Network tab for CORS, HTTP errors, wrong ports or mixed HTTP/HTTPS. This repository does not modify backend CORS settings. For a simulator opened on another device, localhost refers to that device; use your backend host address and an allowed origin.

## Hardware path

The canvas backing buffer remains exactly 128×64 pixels, enlarged with nearest-neighbor rendering. It is a visual preview, not an electrical or timing emulator. The [Phase 1 MatrixPortal S3 diagnostic](firmware/matrixportal-s3/README.md) cycles through color bars, a 128×64 grid and a sample scoreboard. The separate [Phase 2 Tempo Wi-Fi scoreboard](firmware/matrixportal-s3/TempoMatrixLive/README.md) adds read-only NBA/NFL/CFB API polling, automatic game rotation, reconnect/retry logic and stale-data fallback. Both sketches await physical hardware validation. Team logos and browser-simulator parity are later phases. The browser loads logo URLs with anonymous CORS and renders them within 32×32 pixels beside each team (48×48 in logo mode), preserving aspect ratio. Missing, failed, timed-out or CORS-blocked images show a team initial. Images are cached by URL with a bounded cache; the canvas remains exactly 128×64. The layout targets the 2 mm pitch 128×64 panel (Adafruit product 6484). Browser image decoding does not implement ESP32 firmware logo support. No commits or pushes are made automatically.

## Team schedules

Paste a full team schedule URL into the API URL field, for example:

- NBA: `http://localhost:4000/api/games/basketball/team/nba/13?season=2027`
- NFL: `http://localhost:4000/api/games/football/team/nfl/17?season=2026`
- CFB: `http://localhost:4000/api/games/football/team/cfb/333?season=2026`

The simulator preserves the complete URL, including season filters or a season year in the path, and rotates through its games. Date/week controls are hidden for full team schedules. Choose a league to return to its scoreboard. Manual JSON also accepts team schedule responses containing a top-level `games` array.

Team rankings appear as `#12 TEAM` when a valid `home.rank` or `away.rank` is supplied, including team schedule responses. Unranked teams omit the prefix. Rankings also appear in ticker and logo modes.
