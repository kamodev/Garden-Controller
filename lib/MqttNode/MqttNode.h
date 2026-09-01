#pragma once
#include <Arduino.h>
#include <PubSubClient.h>
#if defined(ESP32)
  #include <WiFi.h>
#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
#endif

// Wraps Wi-Fi + MQTT connection management for a garden node: connects to
// Wi-Fi, connects to the broker with a Last Will/Testament announcing the
// node offline, publishes an "online" status once connected, and reconnects
// both non-blockingly (via loop()/isConnected()) if the link drops.
class MqttNode {
public:
    MqttNode(const char* wifiSsid, const char* wifiPassword,
             const char* mqttHost, uint16_t mqttPort,
             const char* mqttUsername, const char* mqttPassword,
             const char* clientId,
             const char* statusTopic,
             const char* onlinePayload, const char* offlinePayload);

    // Blocks (up to a timeout) until Wi-Fi is up; call once from setup().
    void begin();

    // Call every loop() iteration: services MQTT's internal loop and
    // transparently reconnects Wi-Fi/MQTT if the connection dropped.
    void loop();

    bool isConnected();

    bool publish(const char* topic, const String& payload, bool retained = false);

private:
    const char* _wifiSsid;
    const char* _wifiPassword;
    const char* _mqttHost;
    uint16_t _mqttPort;
    const char* _mqttUsername;
    const char* _mqttPassword;
    const char* _clientId;
    const char* _statusTopic;
    const char* _onlinePayload;
    const char* _offlinePayload;

    WiFiClient _wifiClient;
    PubSubClient _mqttClient;
    unsigned long _lastReconnectAttempt = 0;

    void connectWifi();
    bool connectMqtt();
};
