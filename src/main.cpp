// Garden Controller node: reads its own battery voltage and publishes it to
// MQTT so the central controller can track battery health across all nodes.
//
// Setup:
//   1. cp include/secrets_example.h include/secrets.h and fill in your
//      Wi-Fi/MQTT credentials.
//   2. Wire a resistor divider from BAT+ to the ADC pin to GND (see
//      include/config.h for the pin and ratio) so the pin never sees more
//      than the board's ADC reference voltage.
//   3. Adjust NODE_ID and the battery thresholds in include/config.h.
//   4. `pio run -t upload` (defaults to the esp32dev environment).
//
// Published payload (JSON) on MQTT_TOPIC_BATTERY, e.g.:
//   {"node":"garden-node-01","voltage":3.87,"percent":62,"low_battery":false,"uptime_s":142}

#include <Arduino.h>
#include <ArduinoJson.h>
#include "config.h"
#include "secrets.h"
#include "BatteryMonitor.h"
#include "MqttNode.h"

static BatteryMonitor batteryMonitor(
    BATTERY_ADC_PIN,
    BATTERY_DIVIDER_RATIO,
    ADC_REF_VOLTAGE,
    BATTERY_VOLTAGE_MIN,
    BATTERY_VOLTAGE_MAX,
    BATTERY_LOW_THRESHOLD_PCT,
    BATTERY_ADC_SAMPLES);

static MqttNode mqttNode(
    WIFI_SSID, WIFI_PASSWORD,
    MQTT_BROKER_HOST, MQTT_BROKER_PORT,
    MQTT_USERNAME, MQTT_PASSWORD,
    NODE_ID,
    MQTT_TOPIC_STATUS,
    MQTT_PAYLOAD_ONLINE, MQTT_PAYLOAD_OFFLINE);

static unsigned long lastPublish = 0;

static void publishBatteryReading() {
    BatteryMonitor::Reading reading = batteryMonitor.read();

    JsonDocument doc;
    doc["node"] = NODE_ID;
    doc["voltage"] = round(reading.voltage * 100.0f) / 100.0f;
    doc["percent"] = reading.percent;
    doc["low_battery"] = reading.lowBattery;
    doc["uptime_s"] = millis() / 1000UL;

    String payload;
    serializeJson(doc, payload);

    if (mqttNode.publish(MQTT_TOPIC_BATTERY, payload)) {
        Serial.printf("[battery] published: %s\n", payload.c_str());
    } else {
        Serial.println("[battery] publish skipped, not connected");
    }

    if (reading.lowBattery) {
        Serial.printf("[battery] WARNING: low battery (%u%%, %.2fV)\n",
                      reading.percent, reading.voltage);
    }
}

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\n[boot] Garden Controller battery monitor starting");

    batteryMonitor.begin();
    mqttNode.begin();

    // Publish an initial reading as soon as we're able to connect, rather
    // than waiting a full interval.
    lastPublish = millis() - BATTERY_PUBLISH_INTERVAL_MS;
}

void loop() {
    mqttNode.loop();

    unsigned long now = millis();
    if (now - lastPublish >= BATTERY_PUBLISH_INTERVAL_MS) {
        lastPublish = now;
        publishBatteryReading();
    }
}
