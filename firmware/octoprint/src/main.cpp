#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include "secret.h"
#include <OctoPrintAPI.h>          
#include <Adafruit_Protomatter.h>  
#include <esp_task_wdt.h>  
#include "config.h"

/**
 * @brief Include secrets only if the file physically exists.
 */
#if __has_include("secret.h")
  #include "secret.h"
  #define HAS_SECRET_FILE
#endif

/**
 * @brief WiFi logic: Fallback to CI flags if macros are not defined via secret.h.
 */
#ifndef SECRET_SSID
    #define SECRET_SSID WIFI_SSID_ENV
#endif
#ifndef SECRET_PASS
    #define SECRET_PASS WIFI_PASS_ENV
#endif

/**
 * @brief API Key logic: Fallback to CI flags.
 */
#ifndef SECRET_API
    #define SECRET_API PRINTER_API_KEY
#endif

// Assignment of final variables (define ONLY ONCE!)
const char* ssid = SECRET_SSID;
const char* password = SECRET_PASS;
const char* octoprint_apikey = SECRET_API;
const char* ip_address = PRINTER_IP; // Sourced directly from platformio.ini
const int octoprint_httpPort = 5000;

// --- Configuration ---
#define HEIGHT 32   
#define WIDTH 64    
#define MAX_FPS 45  

uint8_t rgbPins[] = { 42, 41, 40, 38, 39, 37 };
uint8_t addrPins[] = { 45, 36, 48, 35, 21 };
uint8_t clockPin = 2;
uint8_t latchPin = 47;
uint8_t oePin = 14;

// --- Function Prototypes ---
void connectToWiFi();
void reconnectWiFi();
void displayWiFiOffline();
void displayOctoprintOffline();
void displayPrinterReady(int temp_T0, int temp_T1);
void displayPrinterPrinting(int seconds, float progress, int temp_T0, int temp_T1);
int scaleFloatToInteger(float value);
void printOctoprintDebug();


IPAddress ip;
WiFiClient client;
OctoprintApi* api;

Adafruit_Protomatter matrix(
  WIDTH, 4, 1, rgbPins, sizeof(addrPins), addrPins,
  clockPin, latchPin, oePin, true);

unsigned long lastRequestTime = 0;
const unsigned long requestInterval = 5000; 

/**
 * @brief Initializes serial communication, WiFi, LED matrix, and the OctoPrint API client.
 */
void setup() {
  Serial.begin(115200);
  // Wait for USB debug connection
  delay(3000);
  
  ip.fromString(ip_address);
  api = new OctoprintApi(client, ip, octoprint_httpPort, octoprint_apikey);

  // Watchdog setup for ESP32
  esp_task_wdt_init(5, true); 
  esp_task_wdt_add(NULL); 

  ProtomatterStatus status = matrix.begin();
  Serial.printf("Protomatter Status: %d\n", status);

  connectToWiFi();
}

/**
 * @brief Main loop handling Watchdog reset, WiFi reconnection, and periodic API polling.
 */
void loop() {
  esp_task_wdt_reset();

  if (WiFi.status() != WL_CONNECTED) {
    reconnectWiFi();
    return;
  }

  unsigned long currentMillis = millis();
  if (currentMillis - lastRequestTime >= requestInterval) {
    lastRequestTime = currentMillis;

    // 1. Retrieve printer status
    if (api->getPrinterStatistics()) {
      int temp_T0 = scaleFloatToInteger(api->printerStats.printerTool0TempActual);
      int temp_T1 = scaleFloatToInteger(api->printerStats.printerBedTempActual);

      if (api->printerStats.printerStatePrinting) {
        // 2. Retrieve job data (endpoint in chunkysteveo library: getPrintJob)
        if (api->getPrintJob()) { 
          displayPrinterPrinting(
            api->printJob.estimatedPrintTime, 
            api->printJob.progressCompletion, 
            temp_T0, temp_T1
          );
        }
      } else if (api->printerStats.printerStateready) {
        displayPrinterReady(temp_T0, temp_T1);
      }
      printOctoprintDebug();
    } else {
      displayOctoprintOffline();
      Serial.println("OctoPrint API unreachable.");
    }
  }
}

// --- Helper Functions ---

/**
 * @brief Connects to the configured WiFi network and blocks until connected.
 */
void connectToWiFi() {
  Serial.print("Connecting to WiFi: ");
  Serial.println(SECRET_SSID);
  WiFi.begin(SECRET_SSID, SECRET_PASS);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");
}

/**
 * @brief Disconnects and attempts to reconnect to the WiFi network.
 */
void reconnectWiFi() {
  displayWiFiOffline();
  WiFi.disconnect();
  WiFi.begin(SECRET_SSID, SECRET_PASS);
  delay(2000);
}

/**
 * @brief Scales and rounds a float value to the nearest integer.
 * @param value The float value to be scaled.
 * @return The rounded integer value.
 */
int scaleFloatToInteger(float value) {
  return (int)(value + 0.5f);
}

// --- Graphic Functions ---

/**
 * @brief Displays a "NO WIFI" error message on the LED matrix.
 */
void displayWiFiOffline() {
  matrix.fillScreen(0);
  matrix.setCursor(2, 10);
  matrix.setTextColor(matrix.color565(255, 0, 0));
  matrix.print("NO WIFI");
  matrix.show();
}

/**
 * @brief Displays a "NO API" error message on the LED matrix.
 */
void displayOctoprintOffline() {
  matrix.fillScreen(0);
  matrix.setCursor(2, 10);
  matrix.setTextColor(matrix.color565(255, 165, 0));
  matrix.print("NO API");
  matrix.show();
}

/**
 * @brief Displays the "READY" state along with tool and bed temperatures.
 * @param temp_T0 Current temperature of Tool 0.
 * @param temp_T1 Current temperature of the Bed.
 */
void displayPrinterReady(int temp_T0, int temp_T1) {
  matrix.fillScreen(0);
  matrix.setCursor(2, 2);
  matrix.setTextColor(matrix.color565(0, 255, 0));
  matrix.print("READY");
  
  matrix.setCursor(2, 12);
  matrix.setTextColor(0xFFFF);
  matrix.printf("T:%d B:%d", temp_T0, temp_T1);
  matrix.show();
}

/**
 * @brief Displays the current printing progress, estimated time left, and temperatures.
 * @param seconds Estimated print time remaining in seconds.
 * @param progress Print progress percentage.
 * @param temp_T0 Current temperature of Tool 0.
 * @param temp_T1 Current temperature of the Bed.
 */
void displayPrinterPrinting(int seconds, float progress, int temp_T0, int temp_T1) {
  matrix.fillScreen(0);
  
  // Progress in blue
  matrix.setCursor(2, 2);
  matrix.setTextColor(matrix.color565(0, 0, 255));
  matrix.printf("%.1f%%", progress);
  
  // Time in white
  int h = seconds / 3600;
  int m = (seconds % 3600) / 60;
  matrix.setCursor(2, 12);
  matrix.setTextColor(0xFFFF);
  matrix.printf("%02dh %02dm", h, m);
  
  // Temperatures at the bottom
  matrix.setCursor(2, 22);
  matrix.setTextColor(matrix.color565(150, 150, 150));
  matrix.printf("T:%d B:%d", temp_T0, temp_T1);
  
  matrix.show();
}

/**
 * @brief Prints detailed OctoPrint status and job information to the serial monitor.
 */
void printOctoprintDebug() {
  // Using the variable names from the chunkysteveo library
  Serial.println("\n---------States---------");
  Serial.print("State: "); Serial.println(api->printerStats.printerState);
  Serial.print("Printing: "); Serial.println(api->printerStats.printerStatePrinting);
  
  if (api->printerStats.printerStatePrinting) {
    Serial.print("Progress: "); Serial.print(api->printJob.progressCompletion); Serial.println("%");
    Serial.print("Time Left: "); Serial.print(api->printJob.estimatedPrintTime); Serial.println("s");
  }
  
  Serial.print("Tool0: "); Serial.println(api->printerStats.printerTool0TempActual);
  Serial.print("Bed:   "); Serial.println(api->printerStats.printerBedTempActual);
  Serial.println("------------------------\n");
}