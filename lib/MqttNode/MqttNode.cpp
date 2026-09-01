#include "MqttNode.h"
#include "config.h"

MqttNode::MqttNode(const char* wifiSsid, const char* wifiPassword,
                    const char* mqttHost, uint16_t mqttPort,
                    const char* mqttUsername, const char* mqttPassword,
                    const char* clientId,
                    const char* statusTopic,
                    const char* onlinePayload, const char* offlinePayload)
    : _wifiSsid(wifiSsid),
      _wifiPassword(wifiPassword),
      _mqttHost(mqttHost),
      _mqttPort(mqttPort),
      _mqttUsername(mqttUsername),
      _mqttPassword(mqttPassword),
      _clientId(clientId),
      _statusTopic(statusTopic),
      _onlinePayload(onlinePayload),
      _offlinePayload(offlinePayload),
      _mqttClient(_wifiClient) {}

void MqttNode::begin() {
    _mqttClient.setServer(_mqttHost, _mqttPort);
    connectWifi();
}

void MqttNode::connectWifi() {
    if (WiFi.status() == WL_CONNECTED) return;

    Serial.printf("[wifi] connecting to %s...\n", _wifiSsid);
    WiFi.mode(WIFI_STA);
    WiFi.begin(_wifiSsid, _wifiPassword);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_CONNECT_TIMEOUT_MS) {
        delay(250);
        Serial.print('.');
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[wifi] connected, ip=%s\n", WiFi.localIP().toString().c_str());
    } else {
        Serial.println("[wifi] connection timed out, will retry");
    }
}

bool MqttNode::connectMqtt() {
    const bool haveCreds = _mqttUsername != nullptr && strlen(_mqttUsername) > 0;

    bool ok;
    if (haveCreds) {
        ok = _mqttClient.connect(_clientId, _mqttUsername, _mqttPassword,
                                  _statusTopic, 1, true, _offlinePayload);
    } else {
        ok = _mqttClient.connect(_clientId, nullptr, nullptr,
                                  _statusTopic, 1, true, _offlinePayload);
    }

    if (ok) {
        Serial.println("[mqtt] connected");
        _mqttClient.publish(_statusTopic, _onlinePayload, /*retained=*/true);
    } else {
        Serial.printf("[mqtt] connect failed, rc=%d\n", _mqttClient.state());
    }
    return ok;
}

void MqttNode::loop() {
    connectWifi();

    if (WiFi.status() != WL_CONNECTED) return;

    if (!_mqttClient.connected()) {
        unsigned long now = millis();
        if (now - _lastReconnectAttempt >= MQTT_RECONNECT_INTERVAL_MS) {
            _lastReconnectAttempt = now;
            connectMqtt();
        }
        return;
    }

    _mqttClient.loop();
}

bool MqttNode::isConnected() {
    return WiFi.status() == WL_CONNECTED && _mqttClient.connected();
}

bool MqttNode::publish(const char* topic, const String& payload, bool retained) {
    if (!isConnected()) return false;
    return _mqttClient.publish(topic, payload.c_str(), retained);
}
