// =====================================================================
//  TEST: is the SX1278 LoRa module wired to the ESP32 correctly?
//  (Phase 0 exit test: "each LoRa module is detected over SPI")
//
//  How it works:
//   Every SX1278 chip has a "version register" (address 0x42) that
//   always holds the value 0x12. We read it over SPI:
//     0x12        -> wiring is good, the chip is talking to us
//     0x00 / 0xFF -> no answer: check wires, power, NSS and RST
//   Then we start the radio at 433 MHz to prove the library works too.
//
//  This sketch does NOT transmit, so it is safe even without an antenna.
//  (Still: always fit the antenna before any sketch that transmits.)
// =====================================================================

#include <SPI.h>
#include <LoRa.h>

// ESP32 pins connected to the LoRa module (same as the wiring table)
const int PIN_SCK  = 18;
const int PIN_MISO = 19;
const int PIN_MOSI = 23;
const int PIN_NSS  = 5;    // "chip select": tells the module we are talking to it
const int PIN_RST  = 14;   // reset line
const int PIN_DIO0 = 26;   // module raises this when a packet arrives / is sent

const long FREQUENCY = 433E6;   // 433 MHz, matches the SX1278

uint8_t readRegister(uint8_t address) {
  digitalWrite(PIN_NSS, LOW);                 // select the module
  SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
  SPI.transfer(address & 0x7F);               // top bit 0 = "read"
  uint8_t value = SPI.transfer(0x00);         // module sends the value back
  SPI.endTransaction();
  digitalWrite(PIN_NSS, HIGH);                // deselect
  return value;
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("=== SX1278 wiring check (ESP32) ===");

  // Reset the module so it starts from a known state
  pinMode(PIN_NSS, OUTPUT);
  digitalWrite(PIN_NSS, HIGH);
  pinMode(PIN_RST, OUTPUT);
  digitalWrite(PIN_RST, LOW);
  delay(10);
  digitalWrite(PIN_RST, HIGH);
  delay(10);

  SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_NSS);

  // Step 1: raw SPI check
  uint8_t version = readRegister(0x42);
  Serial.print("Version register = 0x");
  Serial.println(version, HEX);

  if (version == 0x12) {
    Serial.println("OK: SX1278 found. SPI wiring is correct.");
  } else if (version == 0x00 || version == 0xFF) {
    Serial.println("FAIL: no answer from the module.");
    Serial.println("  - Is VCC on 3.3V (NOT 5V) and GND connected?");
    Serial.println("  - Check SCK=18, MISO=19, MOSI=23, NSS=5, RST=14.");
    Serial.println("  - Are MISO and MOSI swapped?");
    return;
  } else {
    Serial.println("WARN: unexpected value - loose wire or wrong chip?");
    return;
  }

  // Step 2: start the radio with the LoRa library
  LoRa.setPins(PIN_NSS, PIN_RST, PIN_DIO0);
  if (LoRa.begin(FREQUENCY)) {
    Serial.println("OK: LoRa radio started at 433 MHz.");
    Serial.println("PHASE 0 TEST PASSED for the gateway side.");
  } else {
    Serial.println("FAIL: LoRa.begin() failed although SPI works - check RST/DIO0.");
  }
}

void loop() {
  // Nothing: this is a one-shot test. Press EN to run it again.
}
