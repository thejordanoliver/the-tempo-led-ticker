# MatrixPortal S3 — Phase 1 hardware bring-up

This is an **offline Arduino test sketch** for a single **Adafruit 128×64 RGB LED Matrix Panel, 2 mm pitch (PID 6484)** driven by **Adafruit Matrix Portal S3 (PID 5778)**. It intentionally does **not** use Wi-Fi, the Tempo API, real game data, credentials, or team-logo assets. The browser simulator is unchanged.

## Hardware and power

- Matrix Portal S3 and 128×64 HUB75 **ABCDE / 1/32-scan** panel.
- USB-C **data** cable to the S3 (USB provides power to the S3 and enables uploading/Serial).
- Panel's supplied HUB75/power connectors, **5V 4A regulated adapter** and **2.1 mm female DC-to-screw-terminal adapter**.

**Unplug power while connecting.** Plug the S3 into the panel's **HUB75 INPUT** (not OUTPUT), with connector orientation correct; use the included data ribbon if required by mounting. Connect the panel's included power cable to the **panel 5V and GND terminals**. Connect this cable's red (+) and black (-) conductors to the matching marked + and - terminals of the female DC jack adapter; verify markings/polarity before inserting the regulated 5V adapter. The **panel gets 5V from its own adapter**, and the **S3 gets USB-C power**. Never connect 5V to the wrong terminal, use a higher-voltage power supply, or use the Matrix Portal screw-terminal **output** as an input. Do not connect/disconnect the HUB75 cable while powered.

The S3's onboard driver uses five row-address signals (ABCDE). Adafruit identifies this board and panel as compatible **without an address-E solder jumper change**. If only half the matrix lights, confirm the exact panel and input orientation before touching jumpers.

## Arduino IDE setup (macOS)

1. Install **Arduino IDE 2**. In **Settings / Preferences → Additional Boards Manager URLs**, add:
   `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
2. Open **Tools → Board → Boards Manager**; install **esp32 by Espressif Systems**. Select **Adafruit MatrixPortal ESP32-S3** under **Tools → Board** (not a generic ESP32-S3 board).
3. From **Tools → Manage Libraries**, install **Adafruit Protomatter** and its required dependency **Adafruit GFX Library** (accept any dependency-install prompts).
4. Open `TempoMatrixBringup/TempoMatrixBringup.ino`. Select the S3's USB serial **Tools → Port**. If available, set **USB CDC On Boot → Enabled** so Serial logs appear on USB.
5. With the panel properly powered, click **Verify**, then **Upload**. If upload/port detection fails, hold **BOOT**, briefly press **RESET**, release **BOOT**, and select the reappearing serial port before retrying. Press **RESET** after uploading if the sketch doesn't start by itself (per Adafruit's S3 guidance).
6. Open **Serial Monitor** at **115200 baud**; press RESET to see startup logs if necessary. Expect `Protomatter begin() status: 0` and `initialization OK`.

## Expected screens

The sketch automatically cycles every **4 / 4 / 8 seconds**:

1. **Colors:** four dim red, green, blue, and white vertical bars, each 32 pixels wide, spanning all 64 rows. They should not be scrambled or mirrored.
2. **Grid:** full 128×64 outer border, vertical lines every 32 pixels, horizontal lines every 16 pixels, readable corner coordinates, and a center `128x64` label. This helps detect missing address rows and orientation problems.
3. **Sample scoreboard:** `TEMPO`, `DEMO`, `BOS 110 - 108 NYK`, and `FINAL`. This is static, illustrative data, not a live score. Text is deliberately simple; matching the browser simulator pixel-for-pixel is a later phase.

In Serial Monitor send `1` (color bars), `2` (grid), `3` (scoreboard), `n` (next), or `h` (help). Selecting a screen resets its auto-advance timer. The port must be configured to send commands without any special line ending, though a newline is harmless.

## Verification checklist

- [ ] Sketch compiles for **Adafruit MatrixPortal ESP32-S3**.
- [ ] Upload succeeds, and serial reports Protomatter status **0** / initialization OK.
- [ ] Entire 128×64 panel illuminates: correct RGB/white bars, no missing 16-row bands.
- [ ] All four grid corners are visible, text is upright, and lines span the whole display.
- [ ] Mock scoreboard shows both scores and `FINAL` without clipping.
- [ ] Screens change automatically and can be selected from Serial Monitor.
- [ ] USB/panel power is stable (no persistent flicker, resets, or excessive heat).

## Troubleshooting

- **No LED output:** confirm 5V power and polarity, USB-C data cable, panel HUB75 **IN** rather than OUT, correct board choice, and Serial Monitor errors. The board does **not** power this entire panel through the IDE upload cable.
- **Only partial rows / scrambled bands:** the panel needs **five address lines**. Check for the precise 128×64 (PID 6484) panel and Matrix Portal **S3** pin compatibility, and inspect input cable seating. Don't solder jumpers as a first troubleshooting step.
- **Wrong colors or reversed display:** check cabling and physical panel orientation. The sketch's colors use standard RGB signals; note any color ordering difference for later firmware.
- **Ghosting, sparkling, or random resets:** confirm a sufficiently rated regulated 5V power supply and correct power wiring; keep the test at its intentionally moderate brightness.
- **Missing serial / upload errors:** try another USB-C data cable or USB port, enable USB CDC On Boot where offered, and use Adafruit's BOOT + RESET recovery procedure.
- **Protomatter status not 0:** capture the numeric status and board/core/library versions from Serial. Do not proceed to networking until hardware initialization is reliable.

## Scope and sources

This has been authored against Adafruit's official pin mappings but **has not been verified on physical hardware** here. No hardware pass, firmware compilation, or upload should be assumed until you run it.

- [Adafruit Matrix Portal S3 — Protomatter / Arduino examples](https://learn.adafruit.com/adafruit-matrixportal-s3/arduino-pixel-dust-demo)
- [Adafruit Protomatter official `simple.ino` pin mapping](https://github.com/adafruit/Adafruit_Protomatter/blob/master/examples/simple/simple.ino)
- [Adafruit panel PID 6484 details and power requirements](https://www.adafruit.com/product/6484)
- [Arduino IDE setup for the Matrix Portal S3](https://learn.adafruit.com/adafruit-matrixportal-s3/arduino-ide-setup)
- [USB/power troubleshooting](https://learn.adafruit.com/adafruit-matrixportal-s3/usb-power)
