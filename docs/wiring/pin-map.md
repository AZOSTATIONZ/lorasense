# Pin map

## Gateway: ESP32 DevKit + SX1278 (Ra-02, 433 MHz)

Status: wired and verified 2026-10-15 (version register read 0x12, `LoRa.begin(433E6)` OK).

| SX1278 | ESP32 | Purpose |
|---|---|---|
| 3.3V | 3V3 | Power (never 5 V) |
| GND | GND | Ground |
| SCK | GPIO 18 | SPI clock |
| MISO | GPIO 19 | SPI data module -> ESP32 |
| MOSI | GPIO 23 | SPI data ESP32 -> module |
| NSS | GPIO 5 | SPI chip select |
| RST | GPIO 14 | Module reset |
| DIO0 | GPIO 26 | RX done / TX done interrupt |

## Node: Arduino Mega 2560 + SX1278 (Ra-02, 433 MHz)

Status: bench wiring. Module powered from the Mega's on-board 3.3V pin with TX power limited to
10 dBm. Mega -> module signals use resistor dividers (Mega pin -> 1 kΩ -> module pin -> 2 kΩ -> GND,
about 3.3 V); SPI slowed to 1 MHz.

**Power note (2026-10-19):** The 4-channel relay module is now in use (D22-D25). The SX1278 still
runs from the Mega's onboard 3.3V regulator. No radio reliability issues have been observed at
10 dBm TX power. If radio dropouts appear under relay switching load, the recommended fix is a
dedicated AMS1117-3.3 regulator for the SX1278 with a 10µF + 100nF decoupling capacitor.

| SX1278 | Mega | Purpose |
|---|---|---|
| 3.3V | 3.3V pin | Power |
| GND | GND | Ground |
| SCK | D52 via 1k/2k divider | SPI clock |
| MISO | D50 (direct) | SPI data module -> Mega |
| MOSI | D51 via 1k/2k divider | SPI data Mega -> module |
| NSS | D53 via 1k/2k divider | SPI chip select |
| RST | D9 via 1k/2k divider | Module reset |
| DIO0 | D2 (direct) | RX done / TX done interrupt |
