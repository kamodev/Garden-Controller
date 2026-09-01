#pragma once
// Non-secret configuration for a Garden Controller node's battery monitor.
// Wi-Fi / MQTT credentials live in include/secrets.h (see secrets_example.h).

// --- Node identity -------------------------------------------------------
// Used to build MQTT topics and to tell nodes apart, e.g. "soil-node-01".
#define NODE_ID "garden-node-01"

// --- MQTT topics -----------------------------------------------------------
// Battery telemetry (JSON), published every BATTERY_PUBLISH_INTERVAL_MS.
#define MQTT_TOPIC_BATTERY  "garden/" NODE_ID "/battery"
// Node availability, set via MQTT Last Will and Testament (retained).
#define MQTT_TOPIC_STATUS   "garden/" NODE_ID "/status"
#define MQTT_PAYLOAD_ONLINE  "online"
#define MQTT_PAYLOAD_OFFLINE "offline"

// --- Battery sense hardware ------------------------------------------------
// ADC pin the battery voltage divider is connected to.
// ESP32: pin 34 is input-only and ADC1, safe to use alongside Wi-Fi.
// ESP8266: only A0 exists.
#if defined(ESP32)
  #define BATTERY_ADC_PIN 34
#elif defined(ESP8266)
  #define BATTERY_ADC_PIN A0
#endif

// Full-scale voltage the ADC pin sees at its max reading (board dependent).
// ESP32 ADC: ~3.3V. ESP8266 A0: ~1.0V (has an on-board divider on most boards,
// but many dev boards like the D1 mini already scale 0-3.3V -> 0-1.0V for you;
// check your board's schematic before trusting this value).
#if defined(ESP32)
  #define ADC_REF_VOLTAGE 3.3f
#elif defined(ESP8266)
  #define ADC_REF_VOLTAGE 1.0f
#endif

// External voltage divider ratio: BATTERY_VOLTAGE = ADC_VOLTAGE * DIVIDER_RATIO.
// Example: two 100k resistors (R1 from BAT+ to ADC pin, R2 from ADC pin to GND)
// halve the voltage, so DIVIDER_RATIO = (R1 + R2) / R2 = 2.0.
#define BATTERY_DIVIDER_RATIO 2.0f

// Single-cell Li-ion/LiPo voltage range used for the percentage estimate.
#define BATTERY_VOLTAGE_MIN 3.0f   // ~0%
#define BATTERY_VOLTAGE_MAX 4.2f   // 100%

// Report low_battery = true at/below this percentage.
#define BATTERY_LOW_THRESHOLD_PCT 20

// Number of ADC samples averaged per reading, to smooth out noise.
#define BATTERY_ADC_SAMPLES 16

// --- Timing ----------------------------------------------------------------
#define BATTERY_PUBLISH_INTERVAL_MS 60000UL   // publish every 60s
#define WIFI_CONNECT_TIMEOUT_MS     15000UL
#define MQTT_RECONNECT_INTERVAL_MS  5000UL
