<div align="center">

# 🕐 Internet Clock v2

**A feature-rich ESP32 smart clock with live weather, news, games, and animations — all on a 128×64 OLED display.**

[![Platform](https://img.shields.io/badge/Platform-ESP32-blue?logo=espressif)](https://www.espressif.com/en/products/socs/esp32)
[![Framework](https://img.shields.io/badge/Framework-Arduino-teal?logo=arduino)](https://www.arduino.cc/)
[![Build](https://img.shields.io/badge/Build-PlatformIO-orange?logo=platformio)](https://platformio.org/)
[![License](https://img.shields.io/badge/License-MIT-green)](LICENSE)

[Inter Clock Demo](demo.mp4)

</div>

---

## ✨ Features

| Screen | What it shows |
|--------|--------------|
| **🕐 Clock** | Large HH:MM display, seconds, full date, WiFi & weather status bar |
| **🌤️ Weather** | Live temperature, feels-like, description, humidity & wind speed |
| **⏱️ Stopwatch** | Precision stopwatch with start/stop and reset |
| **📅 Eid Countdown** | Days remaining until Eid ul-Fitr (configurable date) |
| **📰 News** | Scrolling live BBC World News headlines via RSS |
| **📈 Forecast** | Hourly temperature bar chart (next 8 × 3-hour slots) |
| **🦕 T-Rex Game** | Full Chrome-style T-Rex runner game with lives, pterodactyls & high score |
| **👀 Eye Animation** | Animated robot eyes with 9 different animation modes |
| **🐧 Arch Screen** | Easter egg — "I use Arch btw" with the Arch Linux logo |

---

## 🎬 Demo

The [demo video](demo.mp4) shows all 9 screens in action — clock, weather, stopwatch, Eid countdown, news scroll, forecast chart, T-Rex game, eye animations, and the Arch screen.

---

## 🛠️ Hardware Required

| Component | Details |
|-----------|---------|
| **Microcontroller** | ESP32 (tested on NodeMCU-32S) |
| **Display** | 0.96" or 1.3" SSD1306 OLED — 128×64 px, I²C |
| **RTC Module** | DS3231 (I²C) |
| **Button 1** | Tactile push button — GPIO **35** |
| **Button 2** | Tactile push button — GPIO **34** |
| **Resistors** | 2× 10kΩ pull-up resistors (one per button, to 3.3V) |
| **Resistors** | 2× 4.7kΩ I²C pull-up resistors (SDA & SCL) if needed |

### Wiring

```
ESP32 GPIO 21 (SDA)  ──── OLED SDA  ──── DS3231 SDA
ESP32 GPIO 22 (SCL)  ──── OLED SCL  ──── DS3231 SCL
ESP32 3.3V           ──── OLED VCC  ──── DS3231 VCC
ESP32 GND            ──── OLED GND  ──── DS3231 GND

ESP32 GPIO 35  ──[ Button 1 ]── GND   (10kΩ pull-up to 3.3V)
ESP32 GPIO 34  ──[ Button 2 ]── GND   (10kΩ pull-up to 3.3V)
```

> ⚠️ GPIO 34 and 35 are **input-only** on ESP32 — they have no internal pull-up. External 10kΩ resistors to 3.3V are **required**.

---

## 🎮 Button Controls

| Button | Screen | Action |
|--------|--------|--------|
| **BTN2** (GPIO 34) | **All screens** | Short press → cycle to next screen |
| **BTN1** (GPIO 35) | **Stopwatch** | Short press → Start / Stop |
| **BTN1** (GPIO 35) | **Stopwatch** | Long press (≥ 300 ms) → Reset |
| **BTN1** (GPIO 35) | **T-Rex Game** | Press → Start game / Jump dino |
| **BTN2** (GPIO 34) | **T-Rex Game** | Press → Exit to next screen |
| **BTN1** (GPIO 35) | **Eye Animation** | Press → Cycle to next eye animation |
| **BTN2** (GPIO 34) | **Eye Animation** | Press → Exit to next screen |

### Screen Cycle Order
```
Clock → Weather → Stopwatch → Eid Countdown → News → Forecast → T-Rex → Eye Anim → Arch → Clock …
```

---

## 🚀 Getting Started

### 1. Prerequisites

- [PlatformIO IDE](https://platformio.org/install) (VS Code extension recommended)
- A free [OpenWeatherMap API key](https://openweathermap.org/api) (Current Weather + Forecast)
- WiFi network (2.4 GHz)

### 2. Clone the Repository

```bash
git clone https://github.com/YOUR_USERNAME/internet-clock.git
cd internet-clock
```

### 3. Configure `src/main.cpp`

Open `src/main.cpp` and fill in the **USER CONFIGURATION** section near the top:

```cpp
// ─────────────────────────────────────────────
//  USER CONFIGURATION  ← edit these
// ─────────────────────────────────────────────
const char* WIFI_SSID      = "Your_WiFi_SSID";
const char* WIFI_PASSWORD  = "Your_WiFi_Password";

const char* OWM_API_KEY    = "Your_OpenWeatherMap_API_Key";
const char* CITY_NAME      = "Your_City";          // e.g. "London"
const char* COUNTRY_CODE   = "Your_Country_Code";  // e.g. "GB"

const char* NTP_SERVER     = "pool.ntp.org";
const long  GMT_OFFSET_SEC = Your_GMT_Offset * 3600; // e.g. 6 for UTC+6

// ── Eid ul-Fitr target date ── update each year ──
const int EID_YEAR  = 2026;
const int EID_MONTH = 3;
const int EID_DAY   = 20;
```

### 4. Build & Flash

```bash
# Build the project
pio run

# Upload to your ESP32
pio run --target upload

# Monitor serial output (optional)
pio device monitor --baud 115200
```

---

## 📦 Dependencies

All libraries are automatically installed by PlatformIO via `platformio.ini`:

| Library | Version | Purpose |
|---------|---------|---------|
| `adafruit/Adafruit SSD1306` | ^2.5.16 | OLED display driver |
| `adafruit/Adafruit GFX Library` | ^1.12.5 | Graphics primitives |
| `adafruit/RTClib` | ^2.1.4 | DS3231 RTC communication |
| `bblanchon/ArduinoJson` | ^7.2.2 | JSON parsing (weather & news) |

> All libraries install automatically the first time you run `pio run`. No manual installation needed.

---

## 🌐 APIs Used

| API | Endpoint | Cost | Purpose |
|-----|---------|------|---------|
| **OpenWeatherMap** | `/data/2.5/weather` | Free tier | Live weather data |
| **OpenWeatherMap** | `/data/2.5/forecast` | Free tier | 3-hour forecast (8 slots) |
| **rss2json.com** | `api.rss2json.com` | Free (no key needed) | BBC World News RSS → JSON |
| **NTP** | `pool.ntp.org` | Free | Internet time sync |

---

## 📁 Project Structure

```
Internet Clock/
├── src/
│   ├── main.cpp            # Main application — screens, WiFi, buttons, loop
│   ├── bitmaps.h           # PROGMEM bitmaps (Arch Linux logo, etc.)
│   ├── EyeAnim.h           # Animated robot eye system (9 animations)
│   ├── TrexGame.h          # T-Rex game entry point & splash screen
│   └── trex/               # T-Rex game engine (ported from t-rex-duino)
│       ├── engine.h        # Core rendering engine (BitCanvas, sprites)
│       ├── TrexPlayer.h    # Dinosaur player logic
│       ├── Cactus.h        # Cactus obstacle
│       ├── Pterodactyl.h   # Flying pterodactyl obstacle
│       ├── Ground.h        # Scrolling ground
│       ├── HeartLive.h     # Extra-life heart collectible
│       ├── assets.h        # Sprite asset loader
│       └── assets/         # Raw bitmap sprite data (in PROGMEM)
├── platformio.ini          # PlatformIO build config & dependencies
└── README.md               # This file
```

---

## 🦕 T-Rex Game Details

The T-Rex game is a full port of the Chrome offline dinosaur game, adapted for the SSD1306 OLED:

- **Lives system** — starts with 3 lives; collect heart power-ups for extra lives (up to 5)
- **Increasing difficulty** — FPS ramps up every 256 score points
- **Obstacles** — cacti (multiple variants) + pterodactyls
- **Day/Night mode** — display inverts every 1024 score points
- **Persistent high score** — tracked for the session
- **Game Over screen** — with restart icon; press BTN1 to restart or BTN2 to exit

---

## 👀 Eye Animation Modes

The eye animation screen cycles through **9 animations** using BTN1:

| # | Animation | Description |
|---|-----------|-------------|
| 0 | Wakeup | Eyes gradually open from a line |
| 1 | Reset | Eyes snap to neutral center position |
| 2 | Move Right | Eyes shift right with the dominant eye growing |
| 3 | Move Left | Eyes shift left with the dominant eye growing |
| 4 | Blink Long | Slow blink with 1-second pause |
| 5 | Blink Short | Quick blink |
| 6 | Happy | Lower eyelid animates upward for a happy expression |
| 7 | Sleep | Eyes close to a thin horizontal bar |
| 8 | Saccade Random | 20 rapid random micro-movements |

---

## ⚙️ Customization

### Change City / Country
Update `CITY_NAME` and `COUNTRY_CODE` in the config section. Use [ISO 3166-1 alpha-2](https://en.wikipedia.org/wiki/ISO_3166-1_alpha-2) codes (e.g., `"US"`, `"GB"`, `"BD"`).

### Change Eid Date
Update `EID_YEAR`, `EID_MONTH`, `EID_DAY` to show the countdown for your local Eid date.

### Change News Source
In `fetchNews()`, replace the RSS URL with any RSS feed supported by [rss2json.com](https://rss2json.com).

### Change Screen Order
Edit the `DisplayMode` enum and the `switch` statement in the main `loop()`. The enum order defines the cycle order.

### Change Fetch Intervals
```cpp
const unsigned long WEATHER_INTERVAL  = 10UL * 60UL * 1000UL; // 10 min
const unsigned long FORECAST_INTERVAL = 30UL * 60UL * 1000UL; // 30 min
const unsigned long NEWS_INTERVAL     = 15UL * 60UL * 1000UL; // 15 min
```

---

## 🔍 Troubleshooting

| Problem | Solution |
|---------|---------|
| OLED not displaying | Check I²C address — try `0x3D` if `0x3C` doesn't work. Run an I²C scanner sketch. |
| "RTC not found" | Verify DS3231 wiring on GPIO 21/22. Check pull-up resistors. |
| Weather shows "Loading..." | Confirm API key is valid and city name is correct. Check serial monitor for HTTP errors. |
| Buttons not responding | GPIO 34/35 need external 10kΩ pull-up resistors to 3.3V — they have no internal pull-ups. |
| T-Rex game crashes | Ensure you have enough RAM — reduce `ArduinoJson` document sizes if needed. |
| News not scrolling | rss2json free tier may rate-limit — try increasing `NEWS_INTERVAL`. |
| Screen flickering | Normal for OLED with full-frame refresh. Increase `delay(30)` in the loop slightly. |

---

## 📜 License

This project is released under the **MIT License**. See [LICENSE](LICENSE) for details.

The T-Rex game engine is based on [t-rex-duino](https://github.com/dehre/t-rex-duino) — original work by the respective author, used and modified under its open-source license.

---

## 🙏 Credits

- **T-Rex Game Engine** — ported from [t-rex-duino](https://github.com/dehre/t-rex-duino)
- **Weather & Forecast** — [OpenWeatherMap API](https://openweathermap.org/api)
- **News** — [BBC World News RSS](https://feeds.bbci.co.uk/news/world/rss.xml) via [rss2json.com](https://rss2json.com)
- **NTP Sync** — `pool.ntp.org`
- **Built with** — [PlatformIO](https://platformio.org/) + Arduino framework

---

<div align="center">

Made with ❤️ by **Prottay** | Running on Arch 🐧

</div>
