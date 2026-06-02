# 🔌 Pin Connections — ESP32 ↔ SX1278

> These tables apply to **both** the Transmitter and Receiver unless noted otherwise.

---

## LoRa Module (SX1278) — Both TX and RX

| SX1278 Pin | ESP32 GPIO | Description          |
|------------|------------|----------------------|
| VCC        | 3.3V       | Power (3.3V only!)   |
| GND        | GND        | Ground               |
| SCK        | GPIO 18    | SPI Clock            |
| MOSI       | GPIO 23    | SPI MOSI             |
| MISO       | GPIO 19    | SPI MISO             |
| NSS (CS)   | GPIO 5     | Chip Select          |
| RESET      | GPIO 26    | Module Reset         |
| DIO0       | GPIO 27    | Interrupt (TX/RX done) |

> ⚠️ **Important:** SX1278 operates at **3.3V only**. Do NOT connect VCC to 5V.

---

## TFT Display (2.8" SPI) — Receiver Only

| TFT Pin | ESP32 GPIO | Description       |
|---------|------------|-------------------|
| VCC     | 3.3V       | Power             |
| GND     | GND        | Ground            |
| SCK     | GPIO 12    | SPI Clock         |
| MOSI    | GPIO 11    | SPI MOSI          |
| CS      | GPIO 10    | Chip Select       |
| DC      | GPIO 9     | Data/Command      |
| RST     | GPIO 14    | Reset             |
| BL      | GPIO 13    | Backlight Control |

> Note: The TFT uses a **separate SPI bus** from the LoRa module.

---

## Battery Monitoring — Receiver Only

```
Battery (+) ──┬── [R1] ──┬── ESP32 ADC Pin
              │          │
             [R2]       GND
              │
             GND
```

| Connection          | Detail                               |
|--------------------|--------------------------------------|
| Voltage Divider In  | Battery positive terminal            |
| Voltage Divider Out | ESP32 ADC input pin                  |
| Reference           | GND                                  |
| Purpose             | Scale battery voltage to ADC range   |

Battery voltage is read via ADC and converted to a percentage for the dashboard.

---

## Power System (Both TX and RX)

```
18650 Battery
    ↓
TP4056 Charging Module (Battery IN/OUT)
    ↓
Power Switch
    ↓
MT3608 Boost Converter (Output → 5V or 3.3V)
    ↓
ESP32 VIN / 3.3V + Peripherals
```

---

## Notes

- Antenna **must be attached** to SX1278 before powering on.
- Keep SPI wires short to avoid signal noise.
- Add a 100nF decoupling capacitor near SX1278 VCC pin.
- TP4056 provides battery protection (overcharge / over-discharge).
- MT3608 boosts the battery voltage for stable system operation.

---