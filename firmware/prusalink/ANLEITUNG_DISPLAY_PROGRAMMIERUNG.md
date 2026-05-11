# Anleitung: Display einfach programmieren (USB + OTA)

Kurzanleitung für das Display (ESP32-S3 + Matrix) in diesem Ordner.

## Ziel

- Erstes Flashen per USB
- Danach kabellos per VS Code (OTA)
- IP-Adresse direkt am Display per Knopf anzeigen

## Voraussetzungen

- VS Code + PlatformIO
- Projektordner [firmware/prusalink](.) öffnen
- Display per USB erreichbar (für den ersten Flash)

## 1. Erstes Flashen per USB

1. Passendes Environment wählen, z. B. `prusa_drucker_3`
2. Upload starten

Terminal-Beispiel:

```bash
pio run -e prusa_drucker_3 -t upload
```

Danach ist OTA auf dem Gerät aktiv.

## 2. IP-Adresse am LCD anzeigen

- Am Display den DOWN-Knopf drücken (GPIO7).
- Solange gedrückt wird die IP angezeigt.
- Nach dem Loslassen bleibt die IP noch kurz sichtbar.

## 3. Kabellos per OTA flashen (VS Code)

Nutze die OTA-Environments:

- `prusa_drucker_1_ota`
- `prusa_drucker_2_ota`
- `prusa_drucker_3_ota`
- `prusa_drucker_4_ota`

Diese Environments verwenden feste mDNS-Namen:

- `matrix-d1.local`
- `matrix-d2.local`
- `matrix-d3.local`
- `matrix-d4.local`

Terminal-Beispiel:

```bash
pio run -e prusa_drucker_1_ota -t upload
```

In VS Code:

1. PlatformIO öffnen
2. `Project Tasks -> prusa_drucker_1_ota -> General -> Upload`

## 4. Wenn OTA nicht gefunden wird

Falls mDNS im Netz nicht funktioniert, per IP hochladen:

```bash
pio run -e prusa_drucker_3_ota -t upload --upload-port 10.30.0.120
```

## 5. Häufige Probleme

- USB-Upload geht nicht: Datenkabel prüfen, richtiges Environment wählen
- OTA findet Gerät nicht: gleiche WLAN-SSID prüfen, IP per DOWN-Knopf anzeigen, dann `--upload-port` nutzen
- Display bleibt schwarz: Das kann im Offline-Zustand gewollt sein; IP-Overlay kommt trotzdem per Knopf
