# Anleitung: Display (ESP32-S3 + RGB-Matrix) per WLAN programmieren

Diese Anleitung beschreibt, wie du die **LED-Matrix-Displays** (ESP32-S3 mit 64×32 RGB-Matrix) per **WLAN (OTA)** oder kabelgebunden über **VS Code + PlatformIO** mit neuer Firmware bespielen kannst.

> **Hinweis:** Es geht hier um die Displays selbst, nicht um die 3D-Drucker, die sie überwachen.

---

## 🛠️ Voraussetzungen

### Hardware
- **ESP32-S3-DevKitC-1** (oder kompatibel) mit angeschlossener RGB-Matrix
- **USB-C-Kabel** (für die Erstprogrammierung)
- **Stromversorgung** für das Display (USB oder extern)

### Software
- **Visual Studio Code** mit **PlatformIO Extension**
  - Extension installieren: `Ctrl+Shift+X` → "PlatformIO IDE" suchen → Installieren
- **Git** (optional, zum Klonen des Repositories)

---

## 📦 Projekt einrichten

### 1. Repository klonen oder öffnen

```bash
git clone https://github.com/FREILab/3DPrinter_Matrix_Display.git
```

Oder in VS Code: `Datei → Ordner öffnen` → `firmware/prusalink` auswählen.

### 2. PlatformIO-Projekt öffnen

Nach dem Öffnen des Ordners erkennt PlatformIO automatisch die `platformio.ini` und richtet das Projekt ein.  
Das kann beim ersten Mal einige Minuten dauern (Downloads der Toolchains und Libraries).

---

## 🔌 Kabelgebundenes Flashen (USB)

### 1. ESP32-S3 per USB verbinden

- USB-C-Kabel an den ESP32-S3 und den Computer anschließen
- Der ESP32-S3 wird als serielles Gerät erkannt

### 2. Gewünschte Umgebung auswählen

In VS Code unten in der **PlatformIO-Toolbar** (blaue Leiste) das gewünschte Environment auswählen:

| Environment | Drucker | IP-Adresse |
|---|---|---|
| `prusa_drucker_2` | Drucker 2 | 10.30.0.7 |
| `prusa_drucker_3` | Drucker 3 | 10.30.0.8 |
| `prusa_drucker_4` | Drucker 4 | 10.30.0.74 |
| `prusa_drucker_5` | Drucker 5 | 10.30.0.75 |

**So wählst du ein Environment aus:**
1. Klicke auf das **PlatformIO-Logo** (Ameise) in der linken Seitenleiste
2. Unter **Project Tasks** klappst du das gewünschte Environment auf
3. Oder: Klicke unten rechts in der blauen Leiste auf das aktuelle Environment-Symbol und wähle ein anderes aus

### 3. Kompilieren & Flashen

**Methode A – Über die PlatformIO-Toolbar:**
- Klicke auf den **→ (Pfeil nach rechts)** Button in der blauen Leiste → "Upload and Monitor"

**Methode B – Über das Terminal:**
```bash
pio run -e prusa_drucker_2 -t upload
pio device monitor
```

**Methode C – Über die PlatformIO-Seitenleiste:**
- `Project Tasks → prusa_drucker_2 → General → Upload and Monitor`

### 4. Serielle Ausgabe beobachten

Nach dem Flashen öffnet sich automatisch der **Serial Monitor** (115200 Baud).  
Dort siehst du:
- WLAN-Verbindungsstatus
- API-Abfragen an den Drucker
- Eventuelle Fehlermeldungen

---

## 📡 Over-the-Air (OTA) Update per WLAN

OTA erlaubt es, die Displays **drahtlos** zu programmieren – ohne USB-Kabel.  
Dafür muss einmalig eine **OTA-fähige Firmware** per USB geflasht werden.

### 1. OTA-Firmware vorbereiten

Füge in der `platformio.ini` folgende OTA-Konfiguration **pro Environment** hinzu:

```ini
[env:prusa_drucker_2]
build_flags = 
    ${env.build_flags}
    -D PRUSA_PRINTER_2
    -D CONFIG_NAME_STR=\"Drucker 2\"
    -D PRINTER_IP=\"10.30.0.07\" 
    -D PRUSA_API_KEY=\"${sysenv.PRUSA_API_KEY_D2}\"
upload_protocol = espota
upload_port = 10.30.0.XX   ; ← HIER DIE IP DES DISPLAYS EINTRAGEN!
```

> **Wichtig:** Die IP unter `upload_port` ist die **IP des ESP32-S3-Displays** im Netzwerk, nicht die des Druckers!

### 2. OTA-Bibliothek einbinden (einmalig)

Füge in der `platformio.ini` unter `lib_deps` hinzu:

```ini
lib_deps =
    adafruit/Adafruit Protomatter @ ^1.7.0
    adafruit/Adafruit GFX Library @ ^1.11.9
    bblanchon/ArduinoJson @ ^7.0.0
    arduino-libraries/ArduinoOTA  ; ← OTA-Bibliothek
```

### 3. OTA-Code in `src/main.cpp` ergänzen

Füge am Ende der `setup()`-Funktion folgenden Code ein:

```cpp
#include <ArduinoOTA.h>  // Ganz oben bei den Includes

// In setup(), nach connectToWiFi():
ArduinoOTA.begin();
Serial.println("[OTA] OTA bereit. IP: " + WiFi.localIP().toString());
```

Und in der `loop()`-Funktion:

```cpp
// Ganz am Anfang der loop():
ArduinoOTA.handle();
```

### 4. Erstmalig per USB flashen

Damit OTA funktioniert, muss **einmalig** die Firmware mit OTA-Unterstützung per USB geflasht werden:

```bash
pio run -e prusa_drucker_2 -t upload
```

### 5. Danach per WLAN flashen

Sobald der ESP32-S3 im WLAN ist, kann per OTA geflasht werden:

```bash
pio run -e prusa_drucker_2 -t upload --upload-port 10.30.0.XX
```

Oder in VS Code:
1. Environment auswählen
2. `Project Tasks → prusa_drucker_2 → Platform → Upload` (klicken)
3. PlatformIO fragt nach der IP – IP des Displays eingeben

---

## 🌐 IP-Adressen der Displays herausfinden

### Methode 1: Serial Monitor
Nach dem Flashen per USB zeigt der Serial Monitor die IP an:
```
[WiFi] Connected.
[WiFi] IP Address: 10.30.0.XX
```

### Methode 2: Router-Webinterface
Im Router nach dem Gerät "esp32-s3-devkitc-1" oder ähnlich suchen.

### Methode 3: Feste IP per DHCP-Reservierung
Im Router eine feste IP für die MAC-Adresse des ESP32-S3 reservieren.  
Die MAC-Adresse wird beim ersten Start im Serial Monitor ausgegeben.

---

## 🖥️ IP-Adresse beim Boot auf dem Display anzeigen

Damit du die IP-Adresse des Displays **ohne Serial Monitor** ablesen kannst (z.B. wenn kein USB-Kabel angeschlossen ist), zeigt das Display nach dem WLAN-Verbinden für **4 Sekunden** die eigene IP-Adresse an.

### 1. Funktion in `src/main.cpp` einfügen

Füge diese neue Funktion vor `setup()` ein:

```cpp
/**
 * @brief Zeigt die IP-Adresse des Displays für 4 Sekunden auf der Matrix an.
 *
 * Wird nach erfolgreicher WLAN-Verbindung aufgerufen, damit die IP
 * ohne Serial Monitor abgelesen werden kann.
 */
void displayIPAddress() {
  String ip = WiFi.localIP().toString();

  // Hintergrund schwarz
  matrix.fillScreen(0);
  matrix.setTextWrap(false);
  matrix.setTextSize(1);

  // Rahmen zeichnen (blau)
  matrix.drawRect(0, 0, 64, 32, matrix.color565(0, 0, 255));

  // Text "IP"
  matrix.setTextColor(0xFFFF);  // weiß
  matrix.setCursor(2, 3);
  matrix.print("IP");

  // IP-Adresse anzeigen (groß, zentriert)
  matrix.setTextColor(matrix.color565(0, 255, 255));  // türkis
  matrix.setTextSize(2);

  // IP-Position dynamisch berechnen (Zentrierung)
  int ipWidth = ip.length() * 12;  // ca. 12 Pixel pro Zeichen bei size 2
  int ipX = (64 - ipWidth) / 2;
  if (ipX < 0) {
    // Falls IP zu lang, auf size 1 zurückfallen
    matrix.setTextSize(1);
    ipX = (64 - ip.length() * 6) / 2;
  }
  matrix.setCursor(max(ipX, 0), 14);
  matrix.print(ip);

  // Auf Display übertragen
  matrix.show();

  // 4 Sekunden anzeigen
  delay(4000);
}
```

### 2. Aufruf in `setup()` ergänzen

Füge den Aufruf **nach** `connectToWiFi()` und **vor** dem Watchdog ein:

```cpp
void setup() {
  // ... bestehender Code ...

  // connect to WiFi
  connectToWiFi();

  // === NEU: IP-Adresse 4 Sekunden anzeigen ===
  if (WiFi.status() == WL_CONNECTED) {
    displayIPAddress();
  }

  // Initialize the watchdog timer
  esp_task_wdt_init(5, true);
  esp_task_wdt_add(NULL);
}
```

### 3. Funktionsprototyp nicht vergessen

Füge den Prototypen zu den bestehenden Prototypen hinzu:

```cpp
// --- Funktionsprototypen ---
void displayWiFiOffline();
void connectToWiFi();
void reconnectWiFi();
void displayPrinterPrinting(int time_left, float progress, int tool_temp, int bed_temp);
void displayPrinterReady(int tool_temp, int bed_temp);
void displayPrusaLinkOffline();
void displayIPAddress();          // ← NEU
int scaleFloatToInteger(float progress);
void printPrusaLinkDebug();
// ---------------------------
```

### Ergebnis

Nach dem Einschalten und erfolgreicher WLAN-Verbindung zeigt das Display:

```
┌──────────────────────────────┐
│ IP                           │
│                              │
│      10.30.0.42              │
│                              │
└──────────────────────────────┘
```

Nach 4 Sekunden wechselt es automatisch zum normalen Druckerstatus-Bildschirm.

---

## ⚙️ Wichtige Konfigurationen

### WLAN-Zugangsdaten

Die WLAN-Daten werden über **Umgebungsvariablen** gesetzt (für CI/CD) oder in `include/secret.h`:

```cpp
// include/secret.h
#define SECRET_SSID "FLIntern"
#define SECRET_PASS "aGA2EEWbk8RTlkQgyoxX"
```

### API-Keys

Die API-Keys für die Drucker werden ebenfalls in `secret.h` oder als Umgebungsvariable gesetzt:

```bash
# .bashrc / .zshrc (für lokale Entwicklung)
export PRUSA_API_KEY_D2="i727i3YRVnPbpvD"
export PRUSA_API_KEY_D3="dein_key_hier"
export PRUSA_API_KEY_D4="dein_key_hier"
export PRUSA_API_KEY_D5="LOKALER_KEY_DRUCKER_5"
```

---

## 🚀 Schnellstart (für ein neues Display)

1. **ESP32-S3 per USB verbinden**
2. **In VS Code:** `prusalink`-Ordner öffnen
3. **Environment wählen:** Z.B. `prusa_drucker_2`
4. **Flashen:** `→ (Upload and Monitor)` Button klicken
5. **IP notieren:** Aus dem Serial Monitor die IP-Adresse ablesen
6. **Fertig!** Das Display zeigt nun den Druckerstatus an

Für zukünftige Updates:
- **OTA:** `pio run -e prusa_drucker_2 -t upload --upload-port <DISPLAY-IP>`
- **Oder:** Wieder per USB-Kabel

---

## ❌ Fehlerbehebung

| Problem | Lösung |
|---|---|
| `Failed to connect to ESP` | USB-Kabel prüfen (Datenkabel nötig!), Board-Treiber installieren |
| `A fatal error occurred: Invalid chip` | Falsches Board ausgewählt → `board = esp32-s3-devkitc-1` prüfen |
| OTA: `No such host` | Falsche IP → `upload_port` prüfen |
| Display bleibt schwarz | Helligkeit prüfen, OE-Pin korrekt? `matrix.begin()`-Status prüfen |
| WLAN verbindet nicht | SSID/Passwort in `secret.h` prüfen |
| "Prusa Link offline" | Drucker-IP und API-Key prüfen, Drucker muss eingeschaltet sein |

---

## 📝 Notizen

- **Matrix-Größe:** Aktuell 64×32 Pixel. Für 64×64: `#define HEIGHT 64` in `src/main.cpp` setzen
- **Watchdog:** 5 Sekunden – bei längeren Operationen wird `esp_task_wdt_reset()` aufgerufen
- **Debug-Modus:** In `src/main.cpp` Zeile 136 auskommentieren: `// prusaLink._debug = true;`
