# Tempo LED Ticker

A standalone 64×32 RGB LED sports ticker project using the existing Tempo sports API. The browser simulator is available now; MatrixPortal S3 firmware will be added after hardware testing.

## Browser simulator

Open `simulator/index.html` in a browser. The NBA endpoint is `http://localhost:4000/api/games/basketball/nba` when your Tempo backend is running on the same Mac. The browser may require CORS support or serving the simulator over HTTP.

To serve the simulator locally:

```bash
cd simulator
python3 -m http.server 8080
```

Then visit http://localhost:8080. If API requests are blocked by CORS, configure an allowed local development origin on your backend; do not disable browser security.

## Hardware target

- Adafruit MatrixPortal S3
- 64×32 HUB75 RGB LED panel (P2.5 target)
- Adafruit Protomatter / Arduino IDE
- 5V power supply rated for the panel

Firmware is **not implemented yet**. A browser rendering is not proof of hardware compatibility.

## Publishing to GitHub

From this directory:

```bash
git init -b main
git add .
git commit -m "Initial LED ticker simulator project"
# Create an empty GitHub repository named tempo-led-ticker, then:
git remote add origin https://github.com/YOUR_USERNAME/tempo-led-ticker.git
git push -u origin main
```

Keep your Tempo backend in its existing repository. Never commit Wi-Fi passwords, tokens, or API keys.

## API client (no backend duplication)

`src/api/tempoApi.mjs` is a small read-only client for the existing Tempo NBA endpoint. It supports an explicit `YYYYMMDD` date and preserves the original game objects. The browser simulator currently has its own built-in fetch/normalization code; this client is a starting point for future shared integration, not yet wired into the simulator or firmware.

Run the API client tests with Node.js 18 or newer:

```bash
node --test tests/*.test.mjs
```

The backend is not copied into this repository, and these tests do not require the backend to be running.
