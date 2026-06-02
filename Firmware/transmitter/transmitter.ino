#include <SPI.h>
#include <LoRa.h>

// ── Pin definitions ───────────────────────────────────────────────────────────
#define LORA_CS     5
#define LORA_RST   14
#define LORA_DIO0  26          
#define LORA_FREQ   433E6

#define LED_PIN     4

// ── TX timing ─────────────────────────────────────────────────────────────────
#define TX_INTERVAL_MS   500UL   // send one packet every 500 ms

// ── State ─────────────────────────────────────────────────────────────────────
uint16_t      packetID       = 0;
unsigned long lastTxTime     = 0;
bool          ledOn          = false;
unsigned long ledOnTime      = 0;
const unsigned long LED_DURATION = 50UL;

void setup() {
    Serial.begin(115200);

    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    // Brief LED self-test on boot
    digitalWrite(LED_PIN, HIGH);
    delay(300);
    digitalWrite(LED_PIN, LOW);

    SPI.begin(18, 19, 23, 5);           
    LoRa.setPins(LORA_CS, LORA_RST, LORA_DIO0);

    Serial.println("Initializing LoRa transmitter...");

    if (!LoRa.begin(LORA_FREQ)) {
        Serial.println("LoRa init FAILED. Check wiring.");
        while (1) {
            digitalWrite(LED_PIN, HIGH); delay(100);
            digitalWrite(LED_PIN, LOW);  delay(100);
        }
    }

    // ── LoRa parameters — must match receiver exactly ────────────────────────
    LoRa.setSpreadingFactor(7);
    LoRa.setSignalBandwidth(125E3);
    LoRa.setCodingRate4(5);
    LoRa.setTxPower(17);
    LoRa.setSyncWord(0xF3);

    Serial.println("LoRa TX ready.");
    Serial.printf("Frequency : %.0f MHz\n", LORA_FREQ / 1E6);
    Serial.printf("TX interval: %lu ms\n", TX_INTERVAL_MS);
}

void loop() {

    unsigned long now = millis();

    // ── Transmit packet ───────────────────────────────────────────────────────
    if (now - lastTxTime >= TX_INTERVAL_MS) {
        lastTxTime = now;

        uint32_t txTime = millis();

        LoRa.beginPacket();
        LoRa.write((uint8_t*)&packetID, 2);
        LoRa.write((uint8_t*)&txTime,   4);
        int result = LoRa.endPacket();

        if (result) {
            digitalWrite(LED_PIN, HIGH);
            ledOn     = true;
            ledOnTime = millis();
            Serial.printf("TX pkt#%u  t=%lums\n", packetID, txTime);
            packetID++;
        } else {
            Serial.printf("TX FAILED pkt#%u\n", packetID);
        }
    }

    // ── Non-blocking LED off ──────────────────────────────────────────────────
    if (ledOn && (millis() - ledOnTime >= LED_DURATION)) {
        digitalWrite(LED_PIN, LOW);
        ledOn = false;
    }
}