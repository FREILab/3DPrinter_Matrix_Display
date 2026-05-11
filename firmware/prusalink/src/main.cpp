/**********************************************************************
 * A sketch to display to read out the state of a Prusa Link         *
 * instance and display it onto a matrix LED display.                 *
 * *
 * Necessary acces data has to be provided to the secret.h            *
 * 08/2025 (Adapted for Prusa Link)                                   *
 * By Marius Tetard for FREILab Freiburg e.V. https://freilab.de      *
 * *
 * This software is open source and licensed under the MIT License.   *
 * *
 * See the LICENSE file or visit https://opensource.org/licenses/MIT  *
 * for more details.                                                  *
 **********************************************************************/


#include <WiFi.h>
#include <WiFiClient.h>

#if __has_include("secret.h")
    #include "secret.h"
#endif

// Fallback für WLAN (falls secret.h im CI leer ist)
#ifndef SECRET_SSID
    #define SECRET_SSID WIFI_SSID_ENV
#endif
#ifndef SECRET_PASS
    #define SECRET_PASS WIFI_PASS_ENV
#endif

#ifndef SECRET_PRUSA_API_KEY
    #define SECRET_PRUSA_API_KEY PRUSA_API_KEY
#endif
#ifndef CONFIG_IP
    #define CONFIG_IP PRINTER_IP
#endif
#ifndef CONFIG_NAME
    #define CONFIG_NAME CONFIG_NAME_STR
#endif
#ifndef CONFIG_PORT
    #define CONFIG_PORT 80
#endif

#include "PrusaLinkAPI.h"
#include <Adafruit_Protomatter.h>
#include <esp_task_wdt.h>


#define HEIGHT 32   // Matrix height (pixels) - SET TO 64 FOR 64x64 MATRIX!
#define WIDTH 64    // Matrix width (pixels)
#define MAX_FPS 45  // Maximum redraw rate, frames/second

// PIN Configuration for the ESP32-S3 and the RGB Matrix
uint8_t rgbPins[] = { 42, 41, 40, 38, 39, 37 };
uint8_t addrPins[] = { 45, 36, 48, 35, 21 };
uint8_t clockPin = 2;
uint8_t latchPin = 47;
uint8_t oePin = 14;

// Define pins for the status RGB LED
#define PIN_RED   A1  // Red channel
#define PIN_GREEN A2  // Green channel
#define PIN_BLUE  A3  // Blue channel

#ifndef IP_SHOW_BUTTON_PIN
  #define IP_SHOW_BUTTON_PIN 0 // Fallback: BOOT/User button (active low)
#endif

#ifndef IP_SHOW_BUTTON_ACTIVE_LOW
  #define IP_SHOW_BUTTON_ACTIVE_LOW 1
#endif

#if defined(BUTTON_DOWN)
  #define IP_SHOW_BUTTON_ALT_PIN BUTTON_DOWN
#elif defined(PIN_BUTTON_DOWN)
  #define IP_SHOW_BUTTON_ALT_PIN PIN_BUTTON_DOWN
#endif

#if HEIGHT == 16
#define NUM_ADDR_PINS 3
#elif HEIGHT == 32
#define NUM_ADDR_PINS 4
#elif HEIGHT == 64
#define NUM_ADDR_PINS 5
#endif

Adafruit_Protomatter matrix(
  WIDTH, 4, 1, rgbPins, NUM_ADDR_PINS, addrPins,
  clockPin, latchPin, oePin, true);
int16_t textX;  // Current text position (X)
int16_t textY;  // Current text position (Y)
char str[64];   // Buffer to text

// Constants for Wi-Fi reconnection
const unsigned long CHECK_INTERVAL = 1000;  // Check Wi-Fi every 1 second
unsigned long previousMillis = 0;

// Setup Prusa Link
WiFiClient client;
// Initialize Prusa Link API. Port 80 is the default for Prusa Link HTTP.
PrusaLinkApi prusaLink(client, (char*)CONFIG_IP, CONFIG_PORT, SECRET_PRUSA_API_KEY);


unsigned long wifiLostSince = 0;
bool wifiWasOffline = false;

unsigned long prusaLinkLostSince = 0;
bool prusaLinkWasOffline = false;


const int tempGood_T0 = 50;   // below this temperature Nozzle is considered cool
const int tempGood_Bed = 50;  // below this temperature Bed is considered cool
const unsigned long IP_BUTTON_AFTERGLOW_MS = 3000;

unsigned long showIpUntil = 0;


// --- Funktionsprototypen ---
void displayWiFiOffline();
void connectToWiFi();
void reconnectWiFi();
void displayPrinterPrinting(int time_left, float progress, int tool_temp, int bed_temp);
void displayPrinterReady(int tool_temp, int bed_temp);
void displayPrusaLinkOffline();
void displayIpAddress();
void drawTinyGlyph(char c, int x, int y, uint16_t color);
void drawTinyText(const String &text, int x, int y, uint16_t color);
bool isPrinterUnavailableState(const char *state);
bool isIpShowButtonPressed();
int scaleFloatToInteger(float progress);
void printPrusaLinkDebug();
// ---------------------------

/**
 * @brief Main setup function, runs once on boot.
 *
 * Initializes the Serial connection for debugging, sets up the GPIO pins for
 * the status LED, starts the RGB matrix display, connects to WiFi, and
 * configures the watchdog timer to prevent crashes.
 */
void setup() {
  // Start Serial Interface
  Serial.begin(115200);
  delay(500);

  // Initialize status LED pins
  pinMode(PIN_RED, OUTPUT);
  pinMode(PIN_GREEN, OUTPUT);
  pinMode(PIN_BLUE, OUTPUT);
  pinMode(IP_SHOW_BUTTON_PIN, INPUT_PULLUP);
#ifdef IP_SHOW_BUTTON_ALT_PIN
  pinMode(IP_SHOW_BUTTON_ALT_PIN, INPUT_PULLUP);
#endif
  
  // Start with the light off
  digitalWrite(PIN_RED, LOW);
  digitalWrite(PIN_GREEN, LOW);
  digitalWrite(PIN_BLUE, LOW);

  Serial.println("[System] Starting up...");
  // enable debug
  // prusaLink._debug = true;

  // Start LED Matrix
  ProtomatterStatus status = matrix.begin();
  Serial.print("[Matrix] Protomatter begin() status: ");
  Serial.println((int)status);

  // Show initial WiFi offline screen
  displayWiFiOffline();

  // connect to WiFi
  connectToWiFi();
  // Initialize the watchdog timer
  esp_task_wdt_init(5, true); // 5 Sekunden Timeout, true = Panic Reset bei Hänger
  esp_task_wdt_add(NULL);     // Fügt den aktuellen Task zur Überwachung hinzu
}

/**
 * @brief Main program loop, runs repeatedly after setup().
 *
 * Checks the WiFi and Prusa Link API status every CHECK_INTERVAL milliseconds.
 * Based on the status, it updates the display and the status LED. It also
 * resets the watchdog timer to indicate the program is running correctly.
 */
void loop() {
  unsigned long currentMillis = millis();

  // IP button: show IP while pressed and keep it visible for a short afterglow.
  if (isIpShowButtonPressed()) {
    showIpUntil = currentMillis + IP_BUTTON_AFTERGLOW_MS;
  }

  if (currentMillis < showIpUntil) {
    displayIpAddress();
    esp_task_wdt_reset();
    return;
  }

  // Check the Wi-Fi connection and API status every second
  if (currentMillis - previousMillis >= CHECK_INTERVAL) {
    previousMillis = currentMillis;
    // Feed the watchdog BEFORE potential long-running operations to prevent a reset
    esp_task_wdt_reset();
    // === Check Wi-Fi connection ===
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("[WiFi] Wi-Fi lost. Reconnecting...");
      if (!wifiWasOffline) {
        wifiLostSince = currentMillis;
        wifiWasOffline = true;
      } else if (currentMillis - wifiLostSince >= 3000) {
        displayWiFiOffline();
        reconnectWiFi();
      }
      return;  // No API calls without WiFi
    } else {
      wifiWasOffline = false;
    }

    Serial.println("[API] Checking Prusa Link...");

    // === Check Prusa Link connection and status ===
    if (prusaLink.getPrinterStatus()) {
      prusaLinkWasOffline = false;
      // Feed the watchdog again after the first successful API call
      esp_task_wdt_reset();
      // PrusaLink can be reachable while the printer itself is disconnected.
      if (prusaLink.printerStats.printerConnectionKnown && !prusaLink.printerStats.printerConnected) {
        displayPrusaLinkOffline();
        Serial.println("[PrusaLink] Printer disconnected (status_printer.ok=false)");
        return;
      }
      // State: Printer is printing
      if (prusaLink.printerStats.printerStatePrinting) {
        if (prusaLink.getJobInfo()) {
          float progress = prusaLink.jobInfo.progressCompletion / 100.0f;
          displayPrinterPrinting(
            prusaLink.jobInfo.progressPrintTimeLeft,
            progress,
            prusaLink.printerStats.printerTool0TempActual,
            prusaLink.printerStats.printerBedTempActual);
        } else {
          Serial.println("[PrusaLink] JobInfo-API offline (but Status is available)");
        }
      }
      // State: Printer is ready/idle
      else if (prusaLink.printerStats.printerStateReady) {
        displayPrinterReady(prusaLink.printerStats.printerTool0TempActual, prusaLink.printerStats.printerBedTempActual);
      }
      // Handle states where PrusaLink is reachable, but the printer is not.
      else if (isPrinterUnavailableState(prusaLink.printerStats.printerState)) {
        displayPrusaLinkOffline();
        Serial.print("[PrusaLink] Printer unavailable state: ");
        Serial.println(prusaLink.printerStats.printerState);
      }
      // Other states (paused, finished, busy, etc.) can be handled here if needed
      else {
        // For now, treat other states like "Ready"
        displayPrinterReady(prusaLink.printerStats.printerTool0TempActual, prusaLink.printerStats.printerBedTempActual);
        Serial.print("[PrusaLink] Printer in unhandled state: ");
        Serial.println(prusaLink.printerStats.printerState);
      }

    } else {
      Serial.println("[PrusaLink] API offline");
      // Optional Debugging lines
      Serial.print("[PrusaLink] HTTP Status Code: ");
      Serial.println(prusaLink.httpStatusCode);
      Serial.print("[PrusaLink] HTTP Error Body: ");
      Serial.println(prusaLink.httpErrorBody);

      // If PrusaLink itself is down (e.g. printer powered off on integrated setups),
      // switch the display off immediately instead of showing stale temperatures.
      displayPrusaLinkOffline();

      if (!prusaLinkWasOffline) {
        prusaLinkLostSince = currentMillis;
        prusaLinkWasOffline = true;
      }
    }
  }

  // Feed the watchdog one last time to be safe
  esp_task_wdt_reset();
}

bool isPrinterUnavailableState(const char *state) {
  if (state == nullptr) {
    return true;
  }

  return (strcmp(state, "OFFLINE") == 0)
         || (strcmp(state, "UNKNOWN") == 0)
         || (strcmp(state, "DISCONNECTED") == 0)
         || (strcmp(state, "N/A") == 0);
}

bool isIpShowButtonPressed() {
  int primaryState = digitalRead(IP_SHOW_BUTTON_PIN);
#if IP_SHOW_BUTTON_ACTIVE_LOW
  bool primaryPressed = (primaryState == LOW);
#else
  bool primaryPressed = (primaryState == HIGH);
#endif

#ifdef IP_SHOW_BUTTON_ALT_PIN
  int altState = digitalRead(IP_SHOW_BUTTON_ALT_PIN);
#if IP_SHOW_BUTTON_ACTIVE_LOW
  bool altPressed = (altState == LOW);
#else
  bool altPressed = (altState == HIGH);
#endif
  return primaryPressed || altPressed;
#else
  return primaryPressed;
#endif
}

/**
 * @brief Handles the initial connection to the WiFi network.
 *
 * Uses the credentials from secret.h to connect to WiFi. It attempts to
 * connect for a fixed number of times before failing. Progress is printed
 * to the Serial monitor.
 */
void connectToWiFi() {
  Serial.println("[WiFi] Connecting to Wi-Fi...");
  WiFi.begin(SECRET_SSID, SECRET_PASS);

  int attempts = 0;
  Serial.print("[WiFi] ");
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {  // Increased attempts
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WiFi] Connected.");
    Serial.println("[WiFi] IP Address: " + WiFi.localIP().toString());
  } else {
    Serial.println("\n[WiFi] Failed to connect.");
  }
}

/**
 * @brief Reconnects to WiFi if the connection is lost.
 *
 * Disconnects from the current network and then attempts to reconnect.
 * This is called when the main loop detects a disconnected state.
 */
void reconnectWiFi() {
  WiFi.disconnect();
  delay(100);
  WiFi.reconnect();
  int attempts = 0;
  Serial.print("[WiFi] Reconnecting...");
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WiFi] Reconnected.");
  } else {
    Serial.println("\n[WiFi] Reconnection attempt failed.");
  }
}

/**
 * @brief Turns the status LED off.
 *
 * Sets the red, green, and blue pins to LOW, turning the light off.
 * This indicates an offline or disconnected state.
 */
void setLightOff() {
  Serial.println("[StatusLight] Setting light to OFF");
  digitalWrite(PIN_RED, LOW);
  digitalWrite(PIN_GREEN, LOW);
  digitalWrite(PIN_BLUE, LOW);
}

/**
 * @brief Sets the status LED to green.
 *
 * Activates only the green pin. This indicates the printer is idle/ready.
 */
void setLightGreen() {
  Serial.println("[StatusLight] Setting light to GREEN");
  digitalWrite(PIN_RED, LOW);
  digitalWrite(PIN_GREEN, HIGH);
  digitalWrite(PIN_BLUE, LOW);
}

/**
 * @brief Sets the status LED to white.
 *
 * Activates the red, green, and blue pins. This indicates the printer is
 * currently printing.
 */
void setLightWhite() {
  Serial.println("[StatusLight] Setting light to WHITE");
  digitalWrite(PIN_RED, HIGH);
  digitalWrite(PIN_GREEN, HIGH);
  digitalWrite(PIN_BLUE, HIGH);
}


/**
 * @brief Displays the printing progress on the matrix.
 *
 * Sets the status light to white. Shows the remaining print time, a progress bar,
 * and the current nozzle and bed temperatures. The border is green.
 * @param seconds The remaining print time in seconds.
 * @param progress The print progress as a float from 0.0 to 1.0.
 * @param temp_T0 The actual temperature of the nozzle.
 * @param temp_Bed The actual temperature of the bed.
 */
void displayPrinterPrinting(int seconds, float progress, int temp_T0, int temp_Bed) {
  setLightWhite();

  int h = seconds / 3600;
  int min = (seconds % 3600) / 60;

  // necessary variables
  int h_ones, h_tens, m_tens, m_ones;
  // Extract digits
  h_tens = h / 10;
  h_ones = h % 10;
  m_tens = min / 10;
  m_ones = min % 10;

  // Fill background black
  matrix.fillScreen(0);
  matrix.setTextWrap(false);
  matrix.setTextSize(1);
  // draw border
  matrix.drawRect(0, 0, 64, 32, matrix.color565(0, 255, 0));  // green

  // Text "Time"
  matrix.setTextColor(0xFFFF);  // white
  matrix.setCursor(2, 3);
  matrix.print("Time");

  // Minutes Text
  matrix.setCursor(57, 3);
  matrix.print("m");

  // Minutes ones
  matrix.setTextColor(matrix.color565(0, 255, 255));  // bright blue
  matrix.setCursor(51, 3);
  matrix.print(m_ones);

  // Minutes tens (if h>0 or m_tens>0)
  if ((h > 0) || (m_tens > 0)) {
    matrix.setCursor(45, 3);
    matrix.print(m_tens);
  }

  // Hours Text (if h>0)
  if (h > 0) {
    matrix.setTextColor(0xFFFF);  // white
    matrix.setCursor(38, 3);
    matrix.print("h");
  }

  // Hours ones (if h>0)
  if (h > 0) {
    matrix.setTextColor(matrix.color565(0, 255, 255));  // bright blue
    matrix.setCursor(32, 3);
    matrix.print(h_ones);
  }

  // Hours tens (if h>9)
  if (h > 9) {
    matrix.setCursor(26, 3);
    matrix.print(h_tens);
  }

  // Draw Progressbar outline
  matrix.drawRect(2, 12, 60, 6, matrix.color565(128, 128, 128));  // gray

  // draw the progress bar length depending on the progress
  int bar_max_progress = scaleFloatToInteger(progress);
  matrix.fillRect(3, 13, bar_max_progress - 3, 4, matrix.color565(0, 255, 0));  // green filled rect

  // Line separating Progress and Temperatures
  matrix.drawRect(1, 20, 62, 1, matrix.color565(255, 255, 255));  // white

  // Display T0 (Nozzle)
  if (temp_T0 < 10) textX = 16;
  else if (temp_T0 < 100) textX = 10;
  else textX = 4;
  textY = 23;
  if (temp_T0 >= tempGood_T0) matrix.setTextColor(matrix.color565(255, 0, 0));  // red
  else matrix.setTextColor(matrix.color565(0, 255, 0));                         // green
  matrix.setCursor(textX, textY);
  matrix.print(temp_T0);
  // display "°C" for T0
  matrix.setTextColor(matrix.color565(255, 255, 255));  // white
  matrix.setCursor(26, 23);
  matrix.print("C");
  matrix.drawCircle(24, 23, 1, matrix.color565(255, 255, 255));  // white

  // Display Slash
  matrix.setCursor(33, 23);
  matrix.print("|");
  // Display T_Bed
  if (temp_Bed < 10) textX = 46;
  else if (temp_Bed < 100) textX = 40;
  else textX = 34;  // For 3-digit bed temps
  textY = 23;
  if (temp_Bed >= tempGood_Bed) matrix.setTextColor(matrix.color565(255, 0, 0));  // red
  else matrix.setTextColor(matrix.color565(0, 255, 0));                           // green
  matrix.setCursor(textX, textY);
  matrix.print(temp_Bed);
  // display "°C" for Bed
  matrix.setTextColor(matrix.color565(255, 255, 255));  // white
  matrix.setCursor(56, 23);
  matrix.print("C");
  matrix.drawCircle(54, 23, 1, matrix.color565(255, 255, 255));  // white

  // Update Display
  matrix.show();
}

/**
 * @brief Displays the printer's ready/idle state on the matrix.
 *
 * Sets the status light to green. Shows "Ready", an empty progress bar, and
 * the current nozzle and bed temperatures. The border is yellow.
 * @param temp_T0 The actual temperature of the nozzle.
 * @param temp_Bed The actual temperature of the bed.
 */
void displayPrinterReady(int temp_T0, int temp_Bed) {
  setLightGreen();

  // Fill background black
  matrix.fillScreen(0);
  matrix.setTextWrap(false);
  matrix.setTextSize(1);
  // draw border
  matrix.drawRect(0, 0, 64, 32, matrix.color565(255, 255, 0));  // yellow

  // Text "Ready"
  matrix.setTextColor(0xFFFF);  // white
  matrix.setCursor(2, 3);
  matrix.print("Ready");

  // Draw Progressbar outline (empty)
  matrix.drawRect(2, 12, 60, 6, matrix.color565(128, 128, 128));  // gray

  // Line separating Progress and Temperatures
  matrix.drawRect(1, 20, 62, 1, matrix.color565(255, 255, 255));  // white

  // Display T0 (Nozzle)
  if (temp_T0 < 10) textX = 16;
  else if (temp_T0 < 100) textX = 10;
  else textX = 4;
  textY = 23;
  if (temp_T0 >= tempGood_T0) matrix.setTextColor(matrix.color565(255, 0, 0));  // red
  else matrix.setTextColor(matrix.color565(0, 255, 0));                         // green
  matrix.setCursor(textX, textY);
  matrix.print(temp_T0);
  // display "°C" for T0
  matrix.setTextColor(matrix.color565(255, 255, 255));  // white
  matrix.setCursor(26, 23);
  matrix.print("C");
  matrix.drawCircle(24, 23, 1, matrix.color565(255, 255, 255));  // white

  // Display Slash
  matrix.setCursor(33, 23);
  matrix.print("|");
  // Display T_Bed
  if (temp_Bed < 10) textX = 46;
  else if (temp_Bed < 100) textX = 40;
  else textX = 34;
  textY = 23;
  if (temp_Bed >= tempGood_Bed) matrix.setTextColor(matrix.color565(255, 0, 0));  // red
  else matrix.setTextColor(matrix.color565(0, 255, 0));                           // green
  matrix.setCursor(textX, textY);
  matrix.print(temp_Bed);
  // display "°C" for Bed
  matrix.setTextColor(matrix.color565(255, 255, 255));  // white
  matrix.setCursor(56, 23);
  matrix.print("C");
  matrix.drawCircle(54, 23, 1, matrix.color565(255, 255, 255));  // white

  // Update Display
  matrix.show();
}

/**
 * @brief Displays a screen indicating Prusa Link is offline.
 *
 * Sets the status light to off. Currently, this function clears the screen
 * to black to indicate an offline state. The commented-out code can be
 * enabled to show a text message instead.
 */
void displayPrusaLinkOffline() {
  setLightOff();

  // Fill background black
  matrix.fillScreen(0);
  matrix.setTextWrap(false);
  matrix.setTextSize(1);
  // keep display off
  /*
  // draw border
  matrix.drawRect(0, 0, 64, 32, matrix.color565(255, 0, 0));  // red

  // Text
  matrix.setTextColor(0xFFFF);  // white
  matrix.setCursor(2, 3);
  matrix.print("Prusa Link");

  matrix.setCursor(2, 14);
  matrix.print("offline");*/

  // Update Display
  matrix.show();
}

/**
 * @brief Displays the current LCD IP address while the user button is pressed.
 *
 * If Wi-Fi is connected, shows the local IP in one line using a tiny pixel
 * font so all octets fit on the 64x32 matrix.
 */
void displayIpAddress() {
  matrix.fillScreen(0);
  matrix.drawRect(0, 0, 64, 32, matrix.color565(0, 128, 255));

  if (WiFi.status() == WL_CONNECTED) {
    String ip = WiFi.localIP().toString();
    int textWidth = ip.length() * 4; // 3 px glyph + 1 px spacing
    int x = (64 - textWidth) / 2;
    if (x < 1) {
      x = 1;
    }
    drawTinyText(ip, x, 13, matrix.color565(0, 255, 255));
  } else {
    drawTinyText("NO WIFI", 18, 13, matrix.color565(255, 180, 0));
  }

  matrix.show();
}

void drawTinyGlyph(char c, int x, int y, uint16_t color) {
  const uint8_t *rows = nullptr;

  static const uint8_t g0[5] = {0x7, 0x5, 0x5, 0x5, 0x7};
  static const uint8_t g1[5] = {0x2, 0x6, 0x2, 0x2, 0x7};
  static const uint8_t g2[5] = {0x7, 0x1, 0x7, 0x4, 0x7};
  static const uint8_t g3[5] = {0x7, 0x1, 0x7, 0x1, 0x7};
  static const uint8_t g4[5] = {0x5, 0x5, 0x7, 0x1, 0x1};
  static const uint8_t g5[5] = {0x7, 0x4, 0x7, 0x1, 0x7};
  static const uint8_t g6[5] = {0x7, 0x4, 0x7, 0x5, 0x7};
  static const uint8_t g7[5] = {0x7, 0x1, 0x1, 0x1, 0x1};
  static const uint8_t g8[5] = {0x7, 0x5, 0x7, 0x5, 0x7};
  static const uint8_t g9[5] = {0x7, 0x5, 0x7, 0x1, 0x7};
  static const uint8_t gd[5] = {0x0, 0x0, 0x0, 0x2, 0x2}; // .
  static const uint8_t gn[5] = {0x5, 0x7, 0x7, 0x7, 0x5}; // N
  static const uint8_t go[5] = {0x0, 0x7, 0x5, 0x5, 0x7}; // O
  static const uint8_t gw[5] = {0x5, 0x5, 0x7, 0x7, 0x5}; // W
  static const uint8_t gi[5] = {0x2, 0x0, 0x2, 0x2, 0x2}; // I
  static const uint8_t gf[5] = {0x7, 0x4, 0x6, 0x4, 0x4}; // F

  switch (c) {
    case '0': rows = g0; break;
    case '1': rows = g1; break;
    case '2': rows = g2; break;
    case '3': rows = g3; break;
    case '4': rows = g4; break;
    case '5': rows = g5; break;
    case '6': rows = g6; break;
    case '7': rows = g7; break;
    case '8': rows = g8; break;
    case '9': rows = g9; break;
    case '.': rows = gd; break;
    case 'N': rows = gn; break;
    case 'O': rows = go; break;
    case 'W': rows = gw; break;
    case 'I': rows = gi; break;
    case 'F': rows = gf; break;
    default: break;
  }

  if (rows == nullptr) {
    return;
  }

  for (int row = 0; row < 5; row++) {
    for (int col = 0; col < 3; col++) {
      if (rows[row] & (1 << (2 - col))) {
        matrix.drawPixel(x + col, y + row, color);
      }
    }
  }
}

void drawTinyText(const String &text, int x, int y, uint16_t color) {
  for (int i = 0; i < text.length(); i++) {
    char c = text[i];
    if (c >= 'a' && c <= 'z') {
      c = c - 'a' + 'A';
    }
    if (c != ' ') {
      drawTinyGlyph(c, x, y, color);
    }
    x += 4;
  }
}

/**
 * @brief Displays a screen indicating WiFi is disconnected.
 *
 * Sets the status light to off. Shows a red border and the text "WiFi offline"
 * to indicate a network connection problem.
 */
void displayWiFiOffline() {
  setLightOff();

  // Fill background black
  matrix.fillScreen(0);
  matrix.setTextWrap(false);
  matrix.setTextSize(1);
  // draw border
  matrix.drawRect(0, 0, 64, 32, matrix.color565(255, 0, 0));  // red

  // Text
  matrix.setTextColor(0xFFFF);  // white
  matrix.setCursor(2, 3);
  matrix.print("WiFi");

  matrix.setCursor(2, 14);
  matrix.print("offline");

  // Update Display
  matrix.show();
}

/**
 * @brief Scales a float value (0.0 to 1.0) to an integer range.
 *
 * This is used to map the print progress percentage to the pixel width
 * of the progress bar on the display.
 * @param value The float value to scale, expected to be between 0.0 and 1.0.
 * @return An integer mapped to the range [3, 61].
 */
int scaleFloatToInteger(float value) {
  // Ensure the input value stays within the expected range
  value = constrain(value, 0.0, 1.0);
  // Map the float to the integer range [3, 61]
  int scaledValue = round(value * (61 - 3) + 3);
  return scaledValue;
}

/**
 * @brief Prints detailed Prusa Link API data to the Serial monitor.
 *
 * This is a debugging function. If called, it fetches the latest status and
 * job information from the printer and prints it in a formatted way.
 */
void printPrusaLinkDebug() {
  if (prusaLink.getPrinterStatus()) {
    Serial.println();
    Serial.println("---------States---------");
    Serial.print("Printer Current State: ");
    Serial.println(prusaLink.printerStats.printerState);
    Serial.print("Printer State - Printing:  ");
    Serial.println(prusaLink.printerStats.printerStatePrinting);
    Serial.print("Printer State - Paused:  ");
    Serial.println(prusaLink.printerStats.printerStatePaused);
    Serial.print("Printer State - Ready:  ");
    Serial.println(prusaLink.printerStats.printerStateReady);
    Serial.println("------------------------");
    Serial.println();
    Serial.println("------Termperatures-----");
    Serial.print("Printer Temp - Nozzle (°C):  ");
    Serial.println(prusaLink.printerStats.printerTool0TempActual);
    Serial.print("Printer Temp - Bed (°C):  ");
    Serial.println(prusaLink.printerStats.printerBedTempActual);
    Serial.println("------------------------");
  }
  if (prusaLink.getJobInfo()) {
    Serial.println();
    Serial.println("----------Job-----------");
    Serial.print("File Name: ");
    Serial.println(prusaLink.jobInfo.jobFileName);
    Serial.print("Progress: ");
    Serial.println(prusaLink.jobInfo.progressCompletion);
    Serial.print("Time Left (s): ");
    Serial.println(prusaLink.jobInfo.progressPrintTimeLeft);
    Serial.println("------------------------");
  }
}