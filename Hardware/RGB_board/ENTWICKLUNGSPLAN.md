# Entwicklungsplan: Aufsteckboard für Adafruit Matrix Portal

## Übersicht

Aufsteckboard (Shield) für das **Adafruit Matrix Portal ESP32-S3**, das folgende Aufgaben übernimmt:

- 12 V Versorgung via Schraubterminals
- Spannungswandlung 12 V → 5 V für das Matrix-Panel und den Matrix Portal
- Anschluss des Status-LED (RGB) via Schraubterminals
- Optional: direkter 5-V-Abgriff via Terminal für weitere Verbraucher

---

## Systemkontext

```
12 V (vom 3D-Drucker-Netzteil)
        │
        ▼
┌───────────────────────────────┐
│        Aufsteckboard          │
│                               │
│  12 V → 5 V Buck Converter    │──── 5 V → Matrix Portal (USB/5V-Pad)
│                               │──── 5 V → LED-Matrix-Panel (Terminal)
│  Status-LED-Treiber           │──── RGB-LED-Stränge (Terminals A1/A2/A3)
│                               │
│  Steckverbinder zum           │
│  Matrix Portal GPIO-Header    │
└───────────────────────────────┘
        │
        ▼
Adafruit Matrix Portal ESP32-S3
        │
        ▼ (HUB75 Ribbon)
64×32 RGB LED Matrix Panel
```

---

## Anforderungen

### Stromversorgung
| Parameter        | Wert                                      |
|-----------------|-------------------------------------------|
| Eingangsspannung | 12 V DC (aus 3D-Drucker-Netzteil)         |
| Ausgangsspannung | 5 V DC                                    |
| Ausgangsstrom    | ≥ 3 A (Matrix Portal ~500 mA + LED-Panel bis ~2 A) |
| Eingangs-Terminal | 2-polig, Rastermaß 5,0 mm                |

### Status-LED-Ausgang (A1/A2/A3 → PIN_RED/GREEN/BLUE)
- 3 Kanäle (R, G, B), aktiv HIGH (laut Code `STATUS_LED_ACTIVE_LOW 0`)
- GPIO-Pegel: 3,3 V, LED-Strom: typ. 10–20 mA
- Schraubterminals für externen LED-Anschluss (z. B. 3- oder 4-polig, 3,5 mm Raster)
- Strombegrenzung durch Vorwiderstände auf der Platine

### Anschluss an Matrix Portal
- Der Matrix Portal ESP32-S3 besitzt zwei Reihen Lötaugen/Header an den Seiten
- Das Aufsteckboard verbindet sich über passende Buchsenleisten
- 5 V wird über den 5V-Anschlusspunkt des Matrix Portals eingespeist

---

## Schaltungsblöcke

### Block 1 – Eingangs-Terminal (12 V)
- 2-poliges **Federklemmen-Terminal** (Push-in, Schlitzschraubendreher), beschriftet `12V` / `GND`
- Verpolungsschutz: P-Kanal MOSFET oder Schottky-Diode in Serie
- Eingangskondensatoren: 100 µF / 25 V Elko + 100 nF Keramik

### Block 2 – Buck Converter 12 V → 5 V
Empfohlenes IC: **AP63203WU** (3 A) oder **AP63205WU** (2 A)
- SOT-23-6 — gut von Hand lötbar
- Feste 5-V-Ausgangsspannung — kein Feedback-Spannungsteiler nötig
- Integrierter High-Side- und Low-Side-MOSFET, integrierte Freilaufdiode → keine externe Schottky

Externe Bauteile (nur 3 Stück):
| Bauteil | Wert | Gehäuse |
|---------|------|---------|
| L1 Induktivität | 4,7–10 µH, I_sat ≥ 3 A | Footprint nach Wahl (z. B. CDRH5D18) |
| C_in Eingangskondensator | 10 µF / 25 V Keramik | 0805 |
| C_out Ausgangskondensator | 22 µF / 10 V Keramik | 0805 |

### Block 3 – 5-V-Ausgangs-Terminal
- 2-poliges **Federklemmen-Terminal** für das LED-Matrix-Panel (5 V / GND)
- Strom bis 3 A

### Block 4 – Status-LED-Treiber (3 Kanäle, SI1308EDL)
Für jeden Kanal (R/G/B) ein **SI1308EDL** (N-Kanal MOSFET, SOT-23, Logic-Level):

```
3,3 V GPIO (A1/A2/A3)
        │
       [Rg 100 Ω]   ← Gate-Schutzwiderstand
        │
       Gate
SI1308EDL
       Drain ──── LED Kathode
       Source ─── GND

5 V ── LED Anode ── (LED-Last) ── LED Kathode
```

- V_GS(th) ≈ 0,5–1,0 V → sicher ansteuerbar mit 3,3-V-GPIO
- I_D(cont.) bis ~3 A → für LED-Stränge mit mehreren LEDs geeignet
- Gate-Widerstand Rg = 100 Ω (verhindert Schwingneigung)
- Gate-Pull-Down 100 kΩ nach GND (MOSFET bleibt sicher aus wenn GPIO hochohmig)
- Vorwiderstand zur LED: auf externen LED-Strang abstimmen (nicht auf Board fest)
- Ausgangsterminal: **Federklemmen-Terminal** pro Kanal (2-polig, Schlitzschraubendreher)
  - Beschriftung: `LED_R+` / `LED_R−`, `LED_G+` / `LED_G−`, `LED_B+` / `LED_B−`
  - `+`-Klemme auf 5 V (direkt vom Buck-Ausgang), `−`-Klemme auf MOSFET-Drain

### Block 5 – Montagelöcher
- 4× M3-Montageloch (Ø 3,2 mm), Kupferfrei (NPTH), in den Ecken des Boards
- Position abgestimmt auf das Matrix Portal bzw. das Gehäuse

### Block 6 – Buchsenleiste zum Matrix Portal
- Entspricht dem GPIO-Header des Matrix Portal ESP32-S3
- Relevante Pins die verbunden werden müssen:
  - `5V` – 5-V-Einspeisung
  - `GND`
  - `A1` (PIN_RED), `A2` (PIN_GREEN), `A3` (PIN_BLUE)

---


## Pinbelegung Matrix Portal ESP32-S3 – Stiftleiste (verifiziert anhand Foto)

Reihenfolge von links nach rechts, 11 Pins:

| # | Pin-Label | GPIO # | Funktion              | Verwendung im Projekt |
|---|-----------|--------|-----------------------|-----------------------|
| 1 | RESET     | –      | Reset (active low)    | –                    |
| 2 | BOOT      | GPIO 0 | Bootloader / User-Taste (active low) | ✅ `IP_SHOW_BUTTON_PIN` |
| 3 | DEBUG     | –      | Hardware-Debug TX     | –                    |
| 4 | TXO       | –      | UART TX               | –                    |
| 5 | RXI       | –      | UART RX               | –                    |
| 6 | A1        | GPIO 3 | ADC / Digital I/O     | ✅ `PIN_RED`   → SI1308EDL Gate R |
| 7 | A2        | GPIO 17| ADC / Digital I/O     | ✅ `PIN_GREEN` → SI1308EDL Gate G |
| 8 | A3        | GPIO 16| ADC / Digital I/O     | ✅ `PIN_BLUE`  → SI1308EDL Gate B |
| 9 | A4        | GPIO 15| ADC / Digital I/O     | –                    |
|10 | 3V        | –      | 3,3 V Ausgang         | –                    |
|11 | GND       | –      | Masse                 | ✅ Masse Aufsteckboard |

> **5V-Einspeisung** erfolgt **nicht** über diese Leiste, sondern über die separaten Schraubterminals / Lötpads des Matrix Portals (seitlich, auf dem Board beschriftet mit „5V" und „GND").

### Power-Pads (separate Schraubterminals auf dem Matrix Portal)

| Pad | Funktion                            | Verwendung im Projekt          |
|-----|-------------------------------------|--------------------------------|
| 5V  | 5 V Einspeisung für Portal + Panel | ✅ Ausgang Buck Converter       |
| GND | Masse                               | ✅ Masse Aufsteckboard          |

> **Zu verbindende Pins auf dem Aufsteckboard:** GND, 5V (Einspeisung über separate Pads), A1 (GPIO3), A2 (GPIO17), A3 (GPIO16). Optional: BOOT/GPIO0 falls ein externer IP-Taster vorgesehen wird.

---

## Bauteil-Kandidaten

| Bauteil              | Vorschlag              | Bemerkung                            |
|---------------------|------------------------|--------------------------------------|
| Buck Converter IC   | AP63203WU / AP63205WU  | SOT-23-6, 3 A / 2 A, 5 V fix, integrierte Diode |
| Induktivität        | z. B. Würth 74408943100 | 10 µH, 3 A, CDRH5D18, handlötbar    |
| C_in                | 10 µF / 25 V / X5R     | 0805 Keramik                         |
| C_out               | 22 µF / 10 V / X5R     | 0805 Keramik                         |
| Federklemme 2-pol (Eingang/Ausgang) | Phoenix PTSM 0,5/ 2-2,5-H o. äquiv. | 12V-Eingang, 5V-Ausgang, bis 4 A |
| Federklemme 2-pol (LEDs, 3×) | Phoenix PTSM 0,5/ 2-2,5-H o. äquiv. | LED-Kanäle R/G/B |
| LED-Treiber MOSFET  | SI1308EDL / SOT-23     | 3× (je R/G/B), Logic-Level N-Kanal  |
| Gate-Widerstand     | 100 Ω / 0603           | Pro MOSFET-Kanal                     |
| Gate-Pull-Down      | 100 kΩ / 0603          | Pro MOSFET-Kanal, GND               |

---

## Offene Fragen

- [ ] Welche Leuchtmittel werden an R/G/B angeschlossen? (Einzelne LEDs, LED-Stränge?) → bestimmt ob Transistorstufe nötig ist und welcher Vorwiderstand
- [ ] Soll ein IP-Taster auf dem Board vorgesehen werden (GPIO 0)?
- [ ] Platinengröße: Soll das Board über den Matrix Portal hinausragen oder bündig sein?
- [ ] Soll es einen Ein-/Ausschalter am Eingang geben?
