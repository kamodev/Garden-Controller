# Garden-Controller
Garden controller for controlling the eco system of nodes that water and montior garden.

## MQTT battery monitor (node firmware)

Firmware for a garden node (ESP32 by default, ESP8266 also supported) that
reads its own battery voltage and publishes it over MQTT so the central
controller can track the battery health of every node in the ecosystem.

### Hardware

Wire a resistor voltage divider from the battery's positive terminal to the
ADC pin, and from the ADC pin to GND, so the pin never sees more than the
board's ADC reference voltage:

```
BAT+ ---[ R1 ]---+---[ R2 ]--- GND
                  |
               ADC pin
```

With two 100k resistors (R1 = R2), the divider halves the voltage
(`BATTERY_DIVIDER_RATIO = 2.0` in `include/config.h`), which comfortably
covers a single-cell Li-ion/LiPo (up to ~4.2V) on an ESP32's 3.3V ADC. Adjust
the ratio in `include/config.h` to match your resistors and battery.

### Setup

1. Install [PlatformIO](https://platformio.org/) (CLI or the VS Code extension).
2. Copy the credentials template and fill in your Wi-Fi/MQTT details:
   ```
   cp include/secrets_example.h include/secrets.h
   ```
   `include/secrets.h` is gitignored so real credentials are never committed.
3. Adjust `NODE_ID`, the ADC pin/divider ratio, and the battery thresholds in
   `include/config.h` for your node and hardware.
4. Build and flash:
   ```
   pio run -t upload            # ESP32 (default environment: esp32dev)
   pio run -e d1_mini -t upload # ESP8266 (e.g. Wemos D1 Mini)
   ```
5. Watch it work: `pio device monitor`

### MQTT topics

| Topic                          | Payload                                                              | Notes |
|---------------------------------|-----------------------------------------------------------------------|-------|
| `garden/<node_id>/battery`     | `{"node":"...","voltage":3.87,"percent":62,"low_battery":false,"uptime_s":142}` | Published every `BATTERY_PUBLISH_INTERVAL_MS` (default 60s). |
| `garden/<node_id>/status`      | `online` / `offline`                                                   | Retained; `offline` is set automatically as an MQTT Last Will if the node drops off unexpectedly. |

`low_battery` becomes `true` once the estimated charge falls to or below
`BATTERY_LOW_THRESHOLD_PCT` (default 20%).
