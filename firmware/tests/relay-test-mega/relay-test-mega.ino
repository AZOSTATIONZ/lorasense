// =====================================================================
//  RELAY TEST - no LoRa, just relays one at a time
//
//  This sketch turns each relay ON for 2 seconds, then OFF, one by one.
//  Watch which ones click and whether ONLY the right one clicks.
//
//  Open Serial Monitor at 115200 and follow along.
// =====================================================================

const uint8_t RELAY_PIN[] = { 22, 23, 24, 25 };
const uint8_t COUNT = 4;
const bool ACTIVE_LOW = true;    // most modules: LOW = relay ON

void relayOn(uint8_t pin)  { digitalWrite(pin, ACTIVE_LOW ? LOW : HIGH); }
void relayOff(uint8_t pin) { digitalWrite(pin, ACTIVE_LOW ? HIGH : LOW); }

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("=== RELAY TEST (no LoRa) ===");

  // All OFF first
  for (uint8_t i = 0; i < COUNT; i++) {
    digitalWrite(RELAY_PIN[i], ACTIVE_LOW ? HIGH : LOW);
    pinMode(RELAY_PIN[i], OUTPUT);
  }
  Serial.println("All relays OFF. Starting test in 3 seconds...");
  delay(3000);
}

void loop() {
  for (uint8_t i = 0; i < COUNT; i++) {
    Serial.print("Relay "); Serial.print(i + 1);
    Serial.print(" (pin D"); Serial.print(RELAY_PIN[i]);
    Serial.println(") -> ON");
    relayOn(RELAY_PIN[i]);
    delay(2000);

    Serial.print("Relay "); Serial.print(i + 1);
    Serial.println(" -> OFF");
    relayOff(RELAY_PIN[i]);
    delay(1000);
  }

  Serial.println("--- All done. Cycling again in 3 seconds... ---");
  Serial.println("Did ONLY the correct relay click each time? (y/n)");
  delay(3000);
}
