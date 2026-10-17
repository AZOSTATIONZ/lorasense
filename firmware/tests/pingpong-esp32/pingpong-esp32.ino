// =====================================================================
//  PING-PONG TEST - GATEWAY side (ESP32)        Phase 1, step 1
//
//  Every 2 seconds:
//    1. send "ping <n>" over LoRa
//    2. wait up to 1.5 s for "pong <n>" from the Mega
//    3. print signal strength (RSSI, SNR) and the success rate
//
//  ⚠ The antenna MUST be fitted - this sketch transmits.
// =====================================================================

#include <SPI.h>
#include <LoRa.h>

const int PIN_SCK  = 18;
const int PIN_MISO = 19;
const int PIN_MOSI = 23;
const int PIN_NSS  = 5;
const int PIN_RST  = 14;
const int PIN_DIO0 = 26;

// Radio settings - BOTH boards must use exactly the same values,
// otherwise they cannot hear each other.
const long FREQUENCY = 433E6;
const int  SPREADING = 9;        // higher = longer range but slower
const long BANDWIDTH = 125E3;
const int  TX_POWER  = 10;       // dBm; low for bench testing (max 20)

const unsigned long PING_EVERY_MS = 2000;
const unsigned long REPLY_WAIT_MS = 1500;

unsigned long counter = 0;
unsigned long replies = 0;

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("=== PING-PONG: gateway (ESP32) ===");

  SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_NSS);
  LoRa.setPins(PIN_NSS, PIN_RST, PIN_DIO0);
  if (!LoRa.begin(FREQUENCY)) {
    Serial.println("FAIL: LoRa module not found. Check wiring.");
    while (true) delay(1000);
  }
  LoRa.setSpreadingFactor(SPREADING);
  LoRa.setSignalBandwidth(BANDWIDTH);
  LoRa.setTxPower(TX_POWER);
  LoRa.enableCrc();              // the radio drops packets that arrive damaged

  Serial.println("Radio ready. Sending pings...");
}

void loop() {
  counter++;
  String ping = "ping " + String(counter);

  // ---- send ----
  LoRa.beginPacket();
  LoRa.print(ping);
  LoRa.endPacket();              // waits until the packet has left the antenna
  Serial.print("-> sent ");
  Serial.print(ping);

  // ---- wait for the matching reply ----
  String expected = "pong " + String(counter);
  unsigned long start = millis();
  bool gotReply = false;

  while (millis() - start < REPLY_WAIT_MS) {
    int size = LoRa.parsePacket();          // 0 = nothing received yet
    if (size > 0) {
      String msg = "";
      while (LoRa.available()) msg += (char)LoRa.read();

      if (msg.startsWith(expected)) {
        gotReply = true;
        replies++;
        Serial.print("   <- got \"");
        Serial.print(msg);
        Serial.print("\"  RSSI ");
        Serial.print(LoRa.packetRssi());    // dBm: -30 very strong ... -120 very weak
        Serial.print(" dBm  SNR ");
        Serial.print(LoRa.packetSnr());     // dB: above 0 = clean signal
        Serial.print(" dB  in ");
        Serial.print(millis() - start);
        Serial.print(" ms");
        break;
      }
    }
  }

  if (!gotReply) Serial.print("   xx no reply");

  Serial.print("   [success ");
  Serial.print(replies);
  Serial.print("/");
  Serial.print(counter);
  Serial.println("]");

  delay(PING_EVERY_MS);
}
