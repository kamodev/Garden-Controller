#include "BatteryMonitor.h"

BatteryMonitor::BatteryMonitor(uint8_t pin,
                               float dividerRatio,
                               float adcRefVoltage,
                               float vMin,
                               float vMax,
                               uint8_t lowThresholdPct,
                               uint8_t samples)
    : _pin(pin),
      _dividerRatio(dividerRatio),
      _adcRefVoltage(adcRefVoltage),
      _vMin(vMin),
      _vMax(vMax),
      _lowThresholdPct(lowThresholdPct),
      _samples(samples > 0 ? samples : 1) {}

void BatteryMonitor::begin() {
#if defined(ESP32)
    // 11dB attenuation gives the ADC roughly a 0-3.3V input range.
    analogSetPinAttenuation(_pin, ADC_11db);
#endif
    pinMode(_pin, INPUT);
}

float BatteryMonitor::readAveragedAdcVoltage() const {
    uint32_t total = 0;
    for (uint8_t i = 0; i < _samples; i++) {
#if defined(ESP32)
        // analogReadMilliVolts() applies the ESP32's factory ADC calibration,
        // which is noticeably more accurate than converting the raw counts
        // ourselves.
        total += analogReadMilliVolts(_pin);
#else
        // Fallback for cores without a calibrated milli-volt helper: convert
        // the raw ADC counts using the configured reference voltage.
        uint32_t raw = analogRead(_pin);
        total += static_cast<uint32_t>((raw * _adcRefVoltage * 1000.0f) / 1023.0f);
#endif
        delay(2);
    }
    float avgMillivolts = static_cast<float>(total) / _samples;
    return avgMillivolts / 1000.0f;
}

uint8_t BatteryMonitor::voltageToPercent(float voltage) const {
    // Simple linear mapping across the configured voltage range. A real
    // Li-ion/LiPo discharge curve is non-linear (it sags fastest at the very
    // top and bottom), so treat this as an estimate, not a precise gauge.
    if (voltage <= _vMin) return 0;
    if (voltage >= _vMax) return 100;
    float pct = (voltage - _vMin) / (_vMax - _vMin) * 100.0f;
    return static_cast<uint8_t>(pct + 0.5f);
}

BatteryMonitor::Reading BatteryMonitor::read() const {
    float adcVoltage = readAveragedAdcVoltage();
    float batteryVoltage = adcVoltage * _dividerRatio;
    uint8_t percent = voltageToPercent(batteryVoltage);

    Reading reading;
    reading.voltage = batteryVoltage;
    reading.percent = percent;
    reading.lowBattery = percent <= _lowThresholdPct;
    return reading;
}
