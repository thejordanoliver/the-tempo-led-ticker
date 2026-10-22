# Tests

`npm test` runs Node unit tests without a backend or browser.

Install development dependencies and Chromium once:

```sh
npm ci
npx playwright install chromium
```

`npm run test:browser` starts an isolated local server on 127.0.0.1:8093, opens Chromium and intercepts API/image requests. No Tempo backend, external image host, credentials or internet access is needed during tests. Keep port 8093 available. `npm run test:all` runs both suites.

The browser suite covers scheduled records, live/halftime/final/overtime layouts, logo loading/cache/fallback, zero/missing scores, long codes, rotation/pause/manual navigation, date/timezone, empty/malformed schedules, stale-data recovery and mobile width. Checked-in PNG baselines compare the actual 128×64 canvas with zero changed pixels. Scores are illustrative fixtures.

On failure, inspect `test-results/` for actual/expected/diff images and retained traces (`npx playwright show-trace PATH_TO_TRACE`). After an intentional display change, run `npm run test:browser:update` and visually review changed baseline PNGs before accepting them. Do not regenerate baselines just to silence a failure.
