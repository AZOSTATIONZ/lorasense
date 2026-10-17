// =====================================================================
//  PING-PONG TEST - NODE side (Arduino Mega 2560)    Phase 1, step 1
//
//  1. On start-up, checks the SX1278 is wired correctly (register 0x42 = 0x12).
//  2. Listens for "ping <n>" from the ESP32.
//  3. Replies "pong <n>" and blinks the built-in LED (pin 13).
//
//  Wiring: SCK, MOSI, NSS and RST go through 1k/2k resistor dividers
//  (5 V -> 3.3 V). MISO and DIO0 connect directly. Module VCC = Mega 3.3V.
//
//  ⚠ The antenna MUST be fitted - this sketch transmits.
// =====================================================================

#include <SPI.h>
#include <LoRa.h>

// Mega hardware SPI pins are fixed: SCK = 52, MISO = 50, MOSI = 51
const int PIN_NSS  = 53;
const int PIN_RST  = 9;
const int PIN_DIO0 = 2;
const int PIN_LED  = 13;

// Must match the ESP32 exactly
const long FREQUENCY = 433E6;
const int  SPREADING = 9;
const long BANDWIDTH = 125E3;
const int  TX_POWER  = 10;       // low: Mega 3.3V pin powers the module

uint8_t readRegister(uint8_t address) {
  digitalWrite(PIN_NSS, LOW);
  SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
  SPI.transfer(address & 0x7F);
  uint8_t value = SPI.transfer(0x00);
  SPI.endTransaction();
  digitalWrite(PIN_NSS, HIGH);
  return value;
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_LED, OUTPUT);
  Serial.println();
  Serial.println("=== PING-PONG: node (Mega) ===");

  // ---- wiring check (same idea as the ESP32 test) ----
  pinMode(PIN_NSS, OUTPUT);
  digitalWrite(PIN_NSS, HIGH);
  pinMode(PIN_RST, OUTPUT);
  digitalWrite(PIN_RST, LOW);
  delay(10);
  digitalWrite(PIN_RST, HIGH);
  delay(10);
  SPI.begin();

  uint8_t version = readRegister(0x42);
  Serial.print("Version register = 0x");
  Serial.println(version, HEX);
  if (version != 0x12) {
    Serial.println("FAIL: module not answering. Check 3.3V, GND, the resistor dividers,");
    Serial.println("      and SCK=52, MISO=50, MOSI=51, NSS=53, RST=9.");
    while (true) delay(1000);
  }
  Serial.println("OK: SX1278 found.");

  // ---- start the radio ----
  // Resistor dividers round off fast signal edges, so talk to the
  // module slowly (1 MHz instead of the default 8 MHz).
  LoRa.setSPIFrequency(1E6);
  LoRa.setPins(PIN_NSS, PIN_RST, PIN_DIO0);
  if (!LoRa.begin(FREQUENCY)) {
    Serial.println("FAIL: LoRa.begin() failed.");
    while (true) delay(1000);
  }
  LoRa.setSpreadingFactor(SPREADING);
  LoRa.setSignalBandwidth(BANDWIDTH);
  LoRa.setTxPower(TX_POWER);
  LoRa.enableCrc();

  Serial.println("Radio ready. Waiting for pings...");
}

void loop() {
  int size = LoRa.parsePacket();
  if (size == 0) return;                     // nothing arrived

  String msg = "";
  while (LoRa.available()) msg += (char)LoRa.read();
  int rssi = LoRa.packetRssi();

  Serial.print("<- got \"");
  Serial.print(msg);
  Serial.print("\"  RSSI ");
  Serial.print(rssi);
  Serial.print(" dBm  SNR ");
  Serial.print(LoRa.packetSnr());
  Serial.print(" dB");

  if (!msg.startsWith("ping ")) {            // ignore anything that isn't ours
    Serial.println("   (ignored)");
    return;
  }

  // Reply with the same number so the ESP32 can match it
  String reply = "pong " + msg.substring(5);
  digitalWrite(PIN_LED, HIGH);
  LoRa.beginPacket();
  LoRa.print(reply);
  LoRa.endPacket();
  digitalWrite(PIN_LED, LOW);

  Serial.print("   -> sent \"");
  Serial.print(reply);
  Serial.println("\"");
}
