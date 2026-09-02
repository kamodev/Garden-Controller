#pragma once
#include <Arduino.h>

// Reads a battery voltage through a resistor-divider on an analog pin and
// converts it into a percentage estimate for a single-cell Li-ion/LiPo cell.
class BatteryMonitor {
public:
    struct Reading {
        float voltage;      // battery voltage in volts, after divider correction
        uint8_t percent;    // 0-100 estimate based on voltage curve
        bool lowBattery;    // true when percent <= low battery threshold
    };

    // pin: ADC pin the voltage divider's midpoint is connected to.
    // dividerRatio: (R1 + R2) / R2, see include/config.h for wiring notes.
    // adcRefVoltage: full-scale voltage of the ADC pin itself (not the battery).
    // vMin/vMax: battery voltage mapped to 0% / 100%.
    // lowThresholdPct: percent at/below which lowBattery is reported true.
    // samples: number of ADC reads averaged per Reading, to reduce noise.
    BatteryMonitor(uint8_t pin,
                   float dividerRatio,
                   float adcRefVoltage,
                   float vMin,
                   float vMax,
                   uint8_t lowThresholdPct = 20,
                   uint8_t samples = 16);

    void begin();

    // Takes a fresh set of ADC samples and returns the resulting reading.
    Reading read() const;

private:
    // Upper bound on `samples`, so readAveragedAdcVoltage() can use a fixed-
    // size stack buffer (no heap allocation) that the ESP32 asm sum loop
    // reads directly.
    static constexpr uint8_t kMaxSamples = 64;

    uint8_t _pin;
    float _dividerRatio;
    float _adcRefVoltage;
    float _vMin;
    float _vMax;
    uint8_t _lowThresholdPct;
    uint8_t _samples;

    float readAveragedAdcVoltage() const;
    uint8_t voltageToPercent(float voltage) const;
};
