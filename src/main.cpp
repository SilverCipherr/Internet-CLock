/*
 * ============================================================
 *  INTERNET CLOCK v2 — ESP32 Project
 *
 *  DISPLAY MODES (cycle with BTN2 short press):
 *    1. Clock          — time, date, WiFi status
 *    2. Weather        — temp, description, humidity, wind
 *    3. Stopwatch      — BTN1 short = start/stop, BTN1 long = reset
 *    4. Eid Countdown  — days until next Eid ul-Fitr
 *    5. News Headlines — scrolling BBC RSS headlines
 *    6. Temp Forecast  — hourly bar chart (next 8 x 3h slots)
 *    7. T-Rex Game     — BTN1 = start game / jump dino
 *                      — BTN2 = exit to next screen
 *    8. Eye Animation  — BTN1 = cycle eye animations
 *                      — BTN2 = exit to next screen
 *    9. Arch Screen    — info screen
 *
 *  BUTTONS (global):
 *    BTN2 (GPIO34) — Short press: cycle to next screen
 *    BTN1 (GPIO35) — Mode-specific action:
 *                      Stopwatch : short = start/stop, long = reset
 *                      T-Rex     : start game / jump dino
 *                      Eye Anim  : cycle through eye animations
 *
 *  === REQUIRED LIBRARIES (Arduino Library Manager) ===
 *  1. Adafruit SSD1306        (Adafruit)
 *  2. Adafruit GFX Library    (Adafruit)
 *  3. RTClib                  (Adafruit)
 *  4. ArduinoJson             (Benoit Blanchon) v6+
 *
 *  === APIs USED ===
 *  - OpenWeatherMap  current weather + 3h forecast (your existing key)
 *  - rss2json.com    free RSS→JSON, no key needed (BBC World News)
 * ============================================================
 */

#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <RTClib.h>
#include <time.h>
#include "bitmaps.h"

// ─────────────────────────────────────────────
//  USER CONFIGURATION  ← edit these
// ─────────────────────────────────────────────
const char* WIFI_SSID      = "Prottay";
const char* WIFI_PASSWORD  = "prottay3698";

const char* OWM_API_KEY    = "c020ea210d6203ca9c8f225f5995d45f";
const char* CITY_NAME      = "Dhaka";
const char* COUNTRY_CODE   = "BD";

const char* NTP_SERVER     = "pool.ntp.org";
const long  GMT_OFFSET_SEC = 6 * 3600;   // UTC+6 Bangladesh
const int   DAYLIGHT_OFFSET = 0;

// ── Eid ul-Fitr target date ── update each year ──
const int EID_YEAR  = 2026;
const int EID_MONTH = 3;
const int EID_DAY   = 20;

// ─────────────────────────────────────────────
//  HARDWARE PINS
// ─────────────────────────────────────────────
#define OLED_SDA   21
#define OLED_SCL   22
#define BTN_MODE   34   // input-only, no internal pull-up — use 10kΩ to 3.3V
#define BTN_SET    35   // input-only, no internal pull-up — use 10kΩ to 3.3V

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT  64
#define OLED_RESET     -1
#define OLED_ADDR    0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
RTC_DS3231 rtc;

// ─────────────────────────────────────────────
//  DISPLAY MODES
// ─────────────────────────────────────────────
enum DisplayMode {
  MODE_CLOCK     = 0,
  MODE_WEATHER   = 1,
  MODE_STOPWATCH = 2,
  MODE_EID       = 3,
  MODE_NEWS      = 4,
  MODE_FORECAST  = 5,
  MODE_TREX      = 6,
  MODE_EYES      = 7,
  MODE_ARCH      = 8,
  MODE_COUNT     = 9
};
DisplayMode currentMode = MODE_CLOCK;

#include "TrexGame.h"

#include "EyeAnim.h"

int current_eye_animation_index = 0;
EyeState left_eye, right_eye;
int corner_radius = 10;

// ─────────────────────────────────────────────
//  WEATHER
// ─────────────────────────────────────────────
String weatherDesc = "Loading...";
float  temperature = 0.0;
float  humidity    = 0.0;
float  windSpeed   = 0.0;
float  feelsLike   = 0.0;

unsigned long lastWeatherFetch   = 0;
const unsigned long WEATHER_INTERVAL = 10UL * 60UL * 1000UL;

// ─────────────────────────────────────────────
//  FORECAST (8 x 3h slots from OWM /forecast)
// ─────────────────────────────────────────────
#define FORECAST_SLOTS 8
float  forecastTemp[FORECAST_SLOTS];
String forecastHour[FORECAST_SLOTS];
bool   forecastReady = false;

unsigned long lastForecastFetch    = 0;
const unsigned long FORECAST_INTERVAL = 30UL * 60UL * 1000UL;

// ─────────────────────────────────────────────
//  NEWS
// ─────────────────────────────────────────────
#define MAX_HEADLINES    5
#define MAX_HEADLINE_LEN 80
char headlines[MAX_HEADLINES][MAX_HEADLINE_LEN];
int  headlineCount   = 0;
bool newsReady       = false;

int  newsScrollX      = SCREEN_WIDTH;
int  currentHeadline  = 0;
unsigned long lastNewsScroll  = 0;
unsigned long lastNewsFetch   = 0;
const unsigned long NEWS_INTERVAL   = 15UL * 60UL * 1000UL;
const unsigned long SCROLL_INTERVAL = 40;

// ─────────────────────────────────────────────
//  STOPWATCH
// ─────────────────────────────────────────────
bool          swRunning   = false;
unsigned long swStartMs   = 0;
unsigned long swElapsedMs = 0;

// ─────────────────────────────────────────────
//  BUTTONS
// ─────────────────────────────────────────────
unsigned long btn1PressTime = 0;
unsigned long btn2PressTime = 0;
bool          btn1WasDown   = false;
bool          btn2WasDown   = false;
const unsigned long LONG_PRESS_MS = 300;
const unsigned long DEBOUNCE_MS   = 50;

// ─────────────────────────────────────────────
//  PROTOTYPES
// ─────────────────────────────────────────────
void connectWiFi();
void syncTimeNTP();
void fetchWeather();
void fetchForecast();
void fetchNews();
void handleButtons();
void drawClockScreen(DateTime& now);
void drawWeatherScreen();
void drawStopwatchScreen();
void drawEidScreen(DateTime& now);
void drawNewsScreen();
void drawForecastScreen();
void drawArchScreen();
void drawCenteredText(const char* txt, int y, uint8_t size = 1);
void showMessage(const char* msg);
void drawNativeBitmap(int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h, uint16_t color);
long daysUntil(DateTime& now, int y, int m, int d);

// ─────────────────────────────────────────────
//  SETUP
// ─────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(400);

  pinMode(BTN_MODE, INPUT);
  pinMode(BTN_SET,  INPUT);

  Wire.begin(OLED_SDA, OLED_SCL);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("OLED init failed");
    while (true);
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextWrap(false); // Fixes scrolling text wrapping issue

  // Splash screen
  drawCenteredText("INTERNET",  12, 2);
  drawCenteredText("CLOCK",  30, 2);
  drawCenteredText("by PROTTAY", 52, 1);
  display.display();
  delay(6000);

  // RTC
  if (!rtc.begin()) {
    Serial.println("RTC not found!");
  } else {
    if (rtc.lostPower()) Serial.println("RTC lost power — will sync NTP");
    Serial.println("RTC OK");
  }

  // Init forecast array
  for (int i = 0; i < FORECAST_SLOTS; i++) {
    forecastTemp[i] = 0;
    forecastHour[i] = "--";
  }

  // WiFi + all initial fetches
  showMessage("Connecting\nWiFi...");
  connectWiFi();

  if (WiFi.status() == WL_CONNECTED) {
    showMessage("Syncing time...");  syncTimeNTP();
    showMessage("Weather...");       fetchWeather();
    showMessage("Forecast...");      fetchForecast();
    showMessage("News...");          fetchNews();
    lastWeatherFetch  = millis();
    lastForecastFetch = millis();
    lastNewsFetch     = millis();
  } else {
    showMessage("WiFi Failed!\nUsing RTC");
    delay(1500);
  }
}

// ─────────────────────────────────────────────
//  MAIN LOOP
// ─────────────────────────────────────────────
void loop() {
  DateTime now = rtc.now();
  unsigned long ms = millis();

  // Periodic background fetches
  if (WiFi.status() == WL_CONNECTED) {
    if (ms - lastWeatherFetch  > WEATHER_INTERVAL)  { fetchWeather();  lastWeatherFetch  = ms; }
    if (ms - lastForecastFetch > FORECAST_INTERVAL) { fetchForecast(); lastForecastFetch = ms; }
    if (ms - lastNewsFetch     > NEWS_INTERVAL)     { fetchNews();     lastNewsFetch     = ms; }
  }

  handleButtons();

  // News scroll ticker
  if (currentMode == MODE_NEWS && newsReady && headlineCount > 0) {
    if (ms - lastNewsScroll > SCROLL_INTERVAL) {
      newsScrollX--;
      lastNewsScroll = ms;
      int textW = strlen(headlines[currentHeadline]) * 6;
      if (newsScrollX < -textW) {
        newsScrollX      = SCREEN_WIDTH;
        currentHeadline  = (currentHeadline + 1) % headlineCount;
      }
    }
  }

  // Render
  display.clearDisplay();
  switch (currentMode) {
    case MODE_CLOCK:     drawClockScreen(now);  break;
    case MODE_WEATHER:   drawWeatherScreen();   break;
    case MODE_STOPWATCH: drawStopwatchScreen(); break;
    case MODE_EID:       drawEidScreen(now);    break;
    case MODE_NEWS:      drawNewsScreen();      break;
    case MODE_FORECAST:  drawForecastScreen();  break;
    case MODE_TREX:      drawTrexScreen();      break;
    case MODE_EYES:      runEyeAnimationLoop(); break;
    case MODE_ARCH:      drawArchScreen();      break;
    default: break;
  }
  display.display();
  delay(30);
}

// ─────────────────────────────────────────────
//  WIFI
// ─────────────────────────────────────────────
void connectWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int t = 0;
  while (WiFi.status() != WL_CONNECTED && t < 24) { delay(500); Serial.print("."); t++; }
  if (WiFi.status() == WL_CONNECTED)
    Serial.println("\nWiFi OK: " + WiFi.localIP().toString());
  else
    Serial.println("\nWiFi FAILED");
}

// ─────────────────────────────────────────────
//  NTP → RTC
// ─────────────────────────────────────────────
void syncTimeNTP() {
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET, NTP_SERVER);
  struct tm t;
  int tries = 0;
  while (!getLocalTime(&t) && tries < 10) { delay(1000); tries++; }
  if (tries < 10) {
    rtc.adjust(DateTime(t.tm_year+1900, t.tm_mon+1, t.tm_mday,
                        t.tm_hour, t.tm_min, t.tm_sec));
    Serial.println("RTC synced from NTP");
  } else {
    Serial.println("NTP sync failed");
  }
}

// ─────────────────────────────────────────────
//  CURRENT WEATHER
// ─────────────────────────────────────────────
void fetchWeather() {
  if (WiFi.status() != WL_CONNECTED) return;
  String url = "http://api.openweathermap.org/data/2.5/weather?q=";
  url += CITY_NAME; url += ","; url += COUNTRY_CODE;
  url += "&appid="; url += OWM_API_KEY; url += "&units=metric";

  HTTPClient http; http.begin(url);
  if (http.GET() == HTTP_CODE_OK) {
    DynamicJsonDocument doc(2048);
    if (!deserializeJson(doc, http.getString())) {
      weatherDesc    = doc["weather"][0]["description"].as<String>();
      weatherDesc[0] = toupper(weatherDesc[0]);
      temperature    = doc["main"]["temp"];
      humidity       = doc["main"]["humidity"];
      windSpeed      = doc["wind"]["speed"];
      feelsLike      = doc["main"]["feels_like"];
      Serial.println("Weather: " + weatherDesc);
    }
  }
  http.end();
}

// ─────────────────────────────────────────────
//  HOURLY FORECAST — OWM /forecast (3h slots)
// ─────────────────────────────────────────────
void fetchForecast() {
  if (WiFi.status() != WL_CONNECTED) return;
  String url = "http://api.openweathermap.org/data/2.5/forecast?q=";
  url += CITY_NAME; url += ","; url += COUNTRY_CODE;
  url += "&appid="; url += OWM_API_KEY;
  url += "&units=metric&cnt="; url += FORECAST_SLOTS;

  HTTPClient http; http.begin(url);
  if (http.GET() == HTTP_CODE_OK) {
    DynamicJsonDocument doc(8192);
    if (!deserializeJson(doc, http.getString())) {
      JsonArray list = doc["list"].as<JsonArray>();
      int i = 0;
      for (JsonObject slot : list) {
        if (i >= FORECAST_SLOTS) break;
        forecastTemp[i] = slot["main"]["temp"];
        // dt_txt = "YYYY-MM-DD HH:MM:SS" — grab hour chars
        String dt = slot["dt_txt"].as<String>();
        forecastHour[i] = dt.substring(11, 13) + "h";
        i++;
      }
      forecastReady = (i > 0);
      Serial.println("Forecast: " + String(i) + " slots");
    }
  }
  http.end();
}

// ─────────────────────────────────────────────
//  NEWS — rss2json.com free tier (no key needed)
//  Feed: BBC News World RSS
// ─────────────────────────────────────────────
void fetchNews() {
  if (WiFi.status() != WL_CONNECTED) return;
  String url = "https://api.rss2json.com/v1/api.json?rss_url=";
  url += "https%3A%2F%2Ffeeds.bbci.co.uk%2Fnews%2Fworld%2Frss.xml";
  // url += "&count=5"; // rss2json free tier no longer supports this parameter

  HTTPClient http; http.begin(url); http.setTimeout(8000);
  if (http.GET() == HTTP_CODE_OK) {
    DynamicJsonDocument doc(12288);
    if (!deserializeJson(doc, http.getString())) {
      JsonArray items = doc["items"].as<JsonArray>();
      headlineCount = 0;
      for (JsonObject item : items) {
        if (headlineCount >= MAX_HEADLINES) break;
        String t = item["title"].as<String>();
        t.replace("&amp;", "&"); t.replace("&quot;", "\""); t.replace("&#39;", "'");
        t.toCharArray(headlines[headlineCount], MAX_HEADLINE_LEN - 1);
        headlines[headlineCount][MAX_HEADLINE_LEN - 1] = '\0';
        headlineCount++;
      }
      newsReady       = (headlineCount > 0);
      newsScrollX     = SCREEN_WIDTH;
      currentHeadline = 0;
      Serial.println("News: " + String(headlineCount) + " headlines");
    }
  }
  http.end();
}

// ─────────────────────────────────────────────
//  BUTTON HANDLER
//  BTN2 (GPIO34) short press → cycle to next screen (all modes)
//  BTN1 (GPIO35) short press → mode-specific action
//                              Stopwatch: start / stop
//  BTN1 (GPIO35) long  press → Stopwatch: reset
//
//  NOTE: MODE_TREX and MODE_EYES run their own blocking loops
//        and handle buttons internally — this handler is only
//        reached for all other modes.
// ─────────────────────────────────────────────
void handleButtons() {
  unsigned long ms = millis();
  bool b1 = (digitalRead(BTN_SET)  == LOW); // BTN1 (pin 35) — action
  bool b2 = (digitalRead(BTN_MODE) == LOW); // BTN2 (pin 34) — screen change

  // ── BTN1 press / release ──────────────────
  if (b1 && !btn1WasDown) { btn1PressTime = ms; btn1WasDown = true; }

  if (!b1 && btn1WasDown) {
    btn1WasDown = false;
    unsigned long held = ms - btn1PressTime;
    if (held < DEBOUNCE_MS) {
      // ignore (noise)
    } else if (currentMode == MODE_STOPWATCH) {
      if (held >= LONG_PRESS_MS) {
        // Long press on stopwatch → reset
        swRunning = false; swElapsedMs = 0; swStartMs = 0;
      } else {
        // Short press on stopwatch → start / stop
        if (swRunning) { swElapsedMs += ms - swStartMs; swRunning = false; }
        else           { swStartMs = ms; swRunning = true; }
      }
    }
    // BTN1 has no action on other info-only screens
  }

  // ── BTN2 press / release — cycle screen ──
  if (b2 && !btn2WasDown) { btn2PressTime = ms; btn2WasDown = true; }

  if (!b2 && btn2WasDown) {
    btn2WasDown = false;
    if (ms - btn2PressTime >= DEBOUNCE_MS) {
      currentMode = (DisplayMode)((currentMode + 1) % MODE_COUNT);
      if (currentMode == MODE_NEWS) { newsScrollX = SCREEN_WIDTH; currentHeadline = 0; }
      Serial.println("Mode: " + String(currentMode));
    }
  }
}

// ─────────────────────────────────────────────
//  SCREEN 1 — CLOCK
// ─────────────────────────────────────────────
void drawClockScreen(DateTime& now) {
  display.setTextSize(1);
  display.setCursor(98, 0);
  display.print("CLOCK");
  display.drawLine(0, 9, SCREEN_WIDTH, 9, SSD1306_WHITE);

  // Big HH:MM
  char tBuf[6];
  snprintf(tBuf, sizeof(tBuf), "%02d:%02d", now.hour(), now.minute());
  int16_t bx, by; uint16_t bw, bh;
  display.setTextSize(3);
  display.getTextBounds(tBuf, 0, 0, &bx, &by, &bw, &bh);
  display.setCursor((SCREEN_WIDTH - bw) / 2, 11);
  display.print(tBuf);

  // Seconds
  char sBuf[5]; snprintf(sBuf, sizeof(sBuf), ":%02d", now.second());
  display.setTextSize(1); display.setCursor(98, 30); display.print(sBuf);

  // Date
  const char* days[]   = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
  const char* months[] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
  char dBuf[22];
  snprintf(dBuf, sizeof(dBuf), "%s %d %s %04d",
           days[now.dayOfTheWeek()], now.day(), months[now.month()-1], now.year());
  drawCenteredText(dBuf, 44);

  // Bottom status bar
  display.setCursor(0, 56);
  display.print(WiFi.status() == WL_CONNECTED ? "W " : "X ");
  char mini[14];
  snprintf(mini, sizeof(mini), "%.0fC %s", temperature, weatherDesc.substring(0,8).c_str());
  display.print(mini);
  display.setCursor(96, 56); display.print("HOLD>");
}

// ─────────────────────────────────────────────
//  SCREEN 2 — WEATHER
// ─────────────────────────────────────────────
void drawWeatherScreen() {
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(CITY_NAME); display.print(", "); display.print(COUNTRY_CODE);
  display.setCursor(108, 0); display.print("WX");
  display.drawLine(0, 10, SCREEN_WIDTH, 10, SSD1306_WHITE);

  // Temperature
  char tBuf[9]; snprintf(tBuf, sizeof(tBuf), "%.1fC", temperature);
  drawCenteredText(tBuf, 13, 2);

  // Feels like
  char fBuf[18]; snprintf(fBuf, sizeof(fBuf), "Feels %.1fC", feelsLike);
  drawCenteredText(fBuf, 34);

  drawCenteredText(weatherDesc.c_str(), 44);

  char iBuf[24]; snprintf(iBuf, sizeof(iBuf), "H:%d%%  W:%.1fm/s", (int)humidity, windSpeed);
  drawCenteredText(iBuf, 54);
}

// ─────────────────────────────────────────────
//  SCREEN 3 — STOPWATCH
// ─────────────────────────────────────────────
void drawStopwatchScreen() {
  drawCenteredText("STOPWATCH", 0);
  display.drawLine(0, 10, SCREEN_WIDTH, 10, SSD1306_WHITE);

  unsigned long total = swElapsedMs + (swRunning ? millis() - swStartMs : 0);
  unsigned long ms    = total % 1000;
  unsigned long secs  = (total / 1000) % 60;
  unsigned long mins  = (total / 60000) % 60;
  unsigned long hrs   = total / 3600000UL;

  if (hrs > 0) {
    char buf[10]; snprintf(buf, sizeof(buf), "%02lu:%02lu:%02lu", hrs, mins, secs);
    drawCenteredText(buf, 14, 2);
  } else {
    char buf[6]; snprintf(buf, sizeof(buf), "%02lu:%02lu", mins, secs);
    drawCenteredText(buf, 13, 3);
  }

  // Milliseconds
  char mBuf[6]; snprintf(mBuf, sizeof(mBuf), ".%03lu", ms);
  display.setTextSize(1); display.setCursor(88, 38); display.print(mBuf);

  // Status + hints
  display.setCursor(0, 50);
  display.print(swRunning ? ">> RUNNING" : "|| PAUSED");
  display.setCursor(0, 57);
  display.print("B1:S/S  HOLD:RST  B2:Next");
}

// ─────────────────────────────────────────────
//  SCREEN 4 — EID COUNTDOWN
// ─────────────────────────────────────────────
void drawEidScreen(DateTime& now) {
  drawCenteredText("EID COUNTDOWN", 0);
  display.drawLine(0, 10, SCREEN_WIDTH, 10, SSD1306_WHITE);

  long days = daysUntil(now, EID_YEAR, EID_MONTH, EID_DAY);

  if (days < 0) {
    drawCenteredText("Eid has passed!", 16);
    drawCenteredText("Update EID_YEAR", 30);
    drawCenteredText("in the code.", 42);
  } else if (days == 0) {
    drawCenteredText("EID", 14, 2);
    drawCenteredText("MUBARAK!", 30, 2);
    drawCenteredText(":)", 54);
  } else {
    char dBuf[6]; snprintf(dBuf, sizeof(dBuf), "%ld", days);
    drawCenteredText(dBuf, 12, 3);

    display.setTextSize(1);
    drawCenteredText(days == 1 ? "day until" : "days until", 42);

    const char* months[] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
    char dateBuf[26];
    snprintf(dateBuf, sizeof(dateBuf), "Eid %d %s %d",
             EID_DAY, months[EID_MONTH-1], EID_YEAR);
    drawCenteredText(dateBuf, 54);
  }
}

// Days from today to target (negative = past)
long daysUntil(DateTime& now, int y, int m, int d) {
  DateTime target(y, m, d, 0, 0, 0);
  DateTime today(now.year(), now.month(), now.day(), 0, 0, 0);
  return ((long)target.unixtime() - (long)today.unixtime()) / 86400L;
}

// ─────────────────────────────────────────────
//  SCREEN 5 — SCROLLING NEWS
// ─────────────────────────────────────────────
void drawNewsScreen() {
  display.setTextSize(1);
  display.setCursor(0, 0); display.print("BBC NEWS");
  if (headlineCount > 0) {
    char cBuf[8]; snprintf(cBuf, sizeof(cBuf), "%d/%d", currentHeadline+1, headlineCount);
    display.setCursor(98, 0); display.print(cBuf);
  }
  display.drawLine(0, 10, SCREEN_WIDTH, 10, SSD1306_WHITE);

  if (!newsReady || headlineCount == 0) {
    drawCenteredText("Fetching news...", 24);
    drawCenteredText("Please wait", 38);
    return;
  }

  // Scrolling headline
  display.setCursor(newsScrollX, 20);
  display.print(headlines[currentHeadline]);

  // Next headline preview when near end
  int textW   = strlen(headlines[currentHeadline]) * 6;
  int nextIdx = (currentHeadline + 1) % headlineCount;
  if (newsScrollX < SCREEN_WIDTH - textW + 30) {
    display.setCursor(0, 40);
    display.print("NEXT: ");
    char prev[18]; strncpy(prev, headlines[nextIdx], 17); prev[17]='\0';
    display.print(prev); display.print("...");
  }

  // Progress bar
  int range    = SCREEN_WIDTH + textW;
  int traveled = constrain(SCREEN_WIDTH - newsScrollX, 0, range);
  int progress = map(traveled, 0, range, 0, SCREEN_WIDTH);
  display.drawRect(0, 57, SCREEN_WIDTH, 6, SSD1306_WHITE);
  display.fillRect(0, 57, progress, 6, SSD1306_WHITE);
}

// ─────────────────────────────────────────────
//  SCREEN 6 — FORECAST BAR CHART
// ─────────────────────────────────────────────
void drawForecastScreen() {
  drawCenteredText("TEMP FORECAST", 0);
  display.drawLine(0, 9, SCREEN_WIDTH, 9, SSD1306_WHITE);

  if (!forecastReady) { drawCenteredText("Fetching...", 28); return; }

  // Find range
  float minT = forecastTemp[0], maxT = forecastTemp[0];
  for (int i = 1; i < FORECAST_SLOTS; i++) {
    if (forecastTemp[i] < minT) minT = forecastTemp[i];
    if (forecastTemp[i] > maxT) maxT = forecastTemp[i];
  }
  float rng = maxT - minT;
  if (rng < 2.0f) rng = 2.0f;

  // Chart area
  const int cX = 18, cY = 11, cW = 108, cH = 38;
  const int bSp = cW / FORECAST_SLOTS;
  const int bW  = bSp - 2;

  // Y-axis labels
  display.setTextSize(1);
  char maxB[5], minB[5];
  snprintf(maxB, sizeof(maxB), "%.0f", maxT);
  snprintf(minB, sizeof(minB), "%.0f", minT);
  display.setCursor(0, cY);     display.print(maxB);
  display.setCursor(0, cY+cH-7); display.print(minB);

  // Axes
  display.drawLine(cX, cY,    cX,    cY+cH, SSD1306_WHITE);
  display.drawLine(cX, cY+cH, cX+cW, cY+cH, SSD1306_WHITE);

  // Bars
  for (int i = 0; i < FORECAST_SLOTS; i++) {
    float norm = (forecastTemp[i] - minT) / rng;
    int   bH   = constrain((int)(norm * (cH - 4)) + 2, 2, cH);
    int   bx   = cX + i * bSp + 1;
    int   by   = cY + cH - bH;

    display.fillRect(bx, by, bW, bH, SSD1306_WHITE);

    // Temp label above bar
    char tL[5]; snprintf(tL, sizeof(tL), "%.0f", forecastTemp[i]);
    int labelY = by - 8;
    if (labelY < cY) labelY = by + 1;
    display.setCursor(bx, labelY);
    display.print(tL);

    // Hour label below axis (every 2nd)
    if (i % 2 == 0) {
      String h = forecastHour[i]; h.replace("h", "");
      display.setCursor(bx, cY + cH + 2);
      display.print(h);
    }
  }

  // Unit
  display.setCursor(120, cY); display.print("C");
}


// ─────────────────────────────────────────────
//  SCREEN 8 — ARCH BTW
// ─────────────────────────────────────────────
void drawArchScreen() {
  // Arch logo takes up a 32x32 block on the left
  drawNativeBitmap(8, 16, arch_logo_bmp, ARCH_LOGO_WIDTH, ARCH_LOGO_HEIGHT, SSD1306_WHITE);
  
  display.setTextSize(1);
  display.setCursor(50, 24);
  display.print("I use");
  
  display.setCursor(50, 36);
  display.print("Arch btw!!!");

  display.setCursor(50, 48);
  display.print("-Prottay");
}


// ─────────────────────────────────────────────
// Draw vertical-byte format bitmap (GLCD format)
// ─────────────────────────────────────────────
void drawNativeBitmap(int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h, uint16_t color) {
  int16_t pages = h / 8;
  for (int16_t p = 0; p < pages; p++) {
    for (int16_t col = 0; col < w; col++) {
      uint8_t b = pgm_read_byte(bitmap + p * w + col);
      for (uint8_t bit = 0; bit < 8; bit++) {
        if (b & (1 << bit)) {
          display.drawPixel(x + col, y + p * 8 + bit, color);
        }
      }
    }
  }
}

// ─────────────────────────────────────────────
//  UTILITIES
// ─────────────────────────────────────────────
void drawCenteredText(const char* txt, int y, uint8_t size) {
  display.setTextSize(size);
  int16_t bx, by; uint16_t bw, bh;
  display.getTextBounds(txt, 0, 0, &bx, &by, &bw, &bh);
  display.setCursor((SCREEN_WIDTH - bw) / 2, y);
  display.print(txt);
}

void showMessage(const char* msg) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 18);
  display.println(msg);
  display.display();
  delay(400);
}