# Entwicklungsplan: Aufsteckboard für Adafruit Matrix Portal

## Übersicht

Aufsteckboard (Shield) für das **Adafruit Matrix Portal ESP32-S3**:

- 12 V Versorgung via Schraubterminals
- Buck Converter 12 V → 5 V für Matrix-Panel und Matrix Portal
- Status-LED-Treiber (RGB) via Schraubterminals
- Optional: 5-V-Abgriff für weitere Verbraucher

---

## Anforderungen

| Parameter        | Wert                                                   |
|-----------------|--------------------------------------------------------|
| Eingangsspannung | 12 V DC (aus 3D-Drucker-Netzteil)                      |
| Ausgangsspannung | 5 V DC, ≥ 3 A (Matrix Portal ~500 mA + Panel bis ~2 A)|
| LED-Kanäle       | 3× RGB, Common-Anode 12 V, aktiv HIGH, 3,3-V-GPIO     |

---

## Schaltungsblöcke

### Block 1 – Spannungsversorgung (J1, 4-polig, Phoenix 1190299)
- Verpolungsschutz: Schottky-Diode **CDBC540-G** (40 V, 5 A, DO-214AB/SMC)

| Pin | Signal  | Funktion              |
|-----|---------|-----------------------|
| 1   | 12V IN  | Eingang vom Netzteil  |
| 2   | GND     | Masse                 |
| 3   | 5V OUT  | Ausgang Buck → Matrix Portal |
| 4   | GND     | Masse                 |

### Block 2 – Buck Converter 12 V → 5 V (TPS563200DDCR)
- Synchron-Schaltregler, 3 A, SOT-23-6, 5 V via R1/R2 eingestellt

| Bauteil       | Wert                    | Gehäuse |
|---------------|-------------------------|---------|
| L1            | 3,3 µH, 6 A             | 7.3×6.6 |
| C_in          | 2× 10 µF, 25 V, X5R     | 0805    |
| C_BST         | 100 nF, 25 V, X7R       | 0805    |
| C_out         | 2× 22 µF, 25 V, X5R     | 0805    |
| R1 (Feedback) | 56,2 kΩ, 1%             | 0805    |
| R2 (Feedback) | 10 kΩ, 1%               | 0805    |

### Block 3 – Status-LED-Treiber (3× SI1308EDL, SOT-23) + LED-Anschluss (J2, 4-polig, Phoenix 1190299)
12-V-RGB-LED, Common-Anode (Low-Side-Schaltung):
- Anode gemeinsam an 12 V; je ein N-Kanal MOSFET schaltet die Kathode (R/G/B) gegen GND
- Gate-Widerstand: 100 Ω; Gate-Pull-Down: 10 kΩ nach GND
- Vorwiderstände für gewünschten LED-Strom auf 12 V auslegen (extern oder on-board TBD)

| Pin | Signal    | Funktion              |
|-----|-----------|-----------------------|
| 1   | +12V      | LED-Anode (gemeinsam) |
| 2   | LED_R_K   | Kathode Rot → MOSFET Drain |
| 3   | LED_G_K   | Kathode Grün → MOSFET Drain |
| 4   | LED_B_K   | Kathode Blau → MOSFET Drain |

### Block 5 – Buchsenleiste / Montagelöcher
- Buchsenleiste entspricht GPIO-Header des Matrix Portal ESP32-S3
- Verbundene Pins: `GND`, `A1` (GPIO3 / PIN_RED), `A2` (GPIO17 / PIN_GREEN), `A3` (GPIO16 / PIN_BLUE)
- 5-V-Einspeisung über separate Schraubterminals des Matrix Portals (nicht über Leiste)
- 4× M3-Montageloch (Ø 3,2 mm, NPTH) in den Ecken

---

## Pinbelegung Matrix Portal ESP32-S3 (11-Pin-Leiste, links → rechts)

| # | Label | GPIO  | Verwendung im Projekt                 |
|---|-------|-------|---------------------------------------|
| 1 | RESET | –     | –                                     |
| 2 | BOOT  | GPIO0 | ✅ `IP_SHOW_BUTTON_PIN`               |
| 3 | DEBUG | –     | –                                     |
| 4 | TXO   | –     | –                                     |
| 5 | RXI   | –     | –                                     |
| 6 | A1    | GPIO3 | ✅ `PIN_RED` → MOSFET Gate R          |
| 7 | A2    | GPIO17| ✅ `PIN_GREEN` → MOSFET Gate G        |
| 8 | A3    | GPIO16| ✅ `PIN_BLUE` → MOSFET Gate B         |
| 9 | A4    | GPIO15| –                                     |
|10 | 3V    | –     | –                                     |
|11 | GND   | –     | ✅ Masse                              |

5-V-Einspeisung über separate Pads `5V` / `GND` seitlich am Matrix Portal.

---

## Stückliste (BOM, Anzahl pro board)

| Pos. | Mouser-Nr.           | Hersteller-Nr.      | Hersteller                | Beschreibung                                       | Menge |
|------|----------------------|---------------------|---------------------------|----------------------------------------------------|------:|
| 1    | 595-TPS563200DDCR    | TPS563200DDCR       | Texas Instruments         | Buck Converter 12→5 V, 3 A, SOT-23-6              | 1     |
| 2    | 791-0805X106M250CT   | 0805X106M250CT      | Walsin                    | C_in Buck – 10 µF, 25 V, X5R, 0805                | 2     |
| 3    | 660-RK73H2ATTD5622F  | RK73H2ATTD5622F     | KOA Speer                 | R_FB1 Buck – 56,2 kΩ, 1%, 0805                    | 1     |
| 4    | 279-1623097-1        | 1623097-1           | TE Connectivity           | R_FB2 Buck + Gate-Pull-Down – 10 kΩ, 1%, 0805     | 4     |
| 5    | 791-0805B104K250CT   | 0805B104K250CT      | Walsin                    | C_BST Buck – 100 nF, 25 V, X7R, 0805              | 1     |
| 6    | 187-CL21A226MAYNNNE  | CL21A226MAYNNNE     | Samsung Electro-Mechanics | C_out Buck – 22 µF, 25 V, X5R, 0805               | 2     |
| 7    | 652-SRP7028CC-3R3M   | SRP7028CC-3R3M      | Bourns                    | L1 Buck – 3,3 µH, 6 A, SMD                        | 1     |
| 8    | 611-CDBC540-G        | CDBC540-G           | Comchip Technology        | Verpolschutz – Schottky-Diode 40 V, 5 A, DO-214AB | 1     |
| 9    | 781-SI1308EDL-T1-GE3 | SI1308EDL-T1-GE3    | Vishay                    | LED-Treiber – N-Kanal MOSFET 30 V, 3 A, SOT-23    | 3     |
| 10   | 603-RC0603FR-07100RL | RC0603FR-07100RL    | Yageo                     | Gate-Widerstand – 100 Ω, 1%, 0603                 | 3     |
| 11   | 651-1190299          | 1190299             | Phoenix Contact           | Federklemme 4-polig (J1: Power, J2: LED)          | 2     |
| 12   | TBD                  | TBD                 | TBD                       | Buchsenleiste 11-polig, 2,54 mm (Matrix Portal)   | 1     |


---

