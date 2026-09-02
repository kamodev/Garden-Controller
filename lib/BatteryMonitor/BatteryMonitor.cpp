#include "BatteryMonitor.h"

#if defined(ESP32)
// ---------------------------------------------------------------------------
// Hand-written Xtensa (ESP32 LX6) inline assembly.
//
// Scope: only the two purely-arithmetic hot spots below are hand-rolled here
// - summing the raw ADC samples, and turning a millivolt reading into a
// clamped 0-100 percentage. Everything that actually touches hardware or a
// protocol stack (the ADC peripheral, Wi-Fi, MQTT, JSON) stays in C/C++:
// those are supplied by the ESP-IDF/Arduino SDK and third-party libraries,
// and hand-writing a WiFi/TCP/MQTT stack in assembly isn't a realistic (or
// beneficial) thing to do here.
//
// This is gated to ESP32 only. The ESP8266 uses a different Xtensa core
// (Tensilica L106) with a more limited instruction set (no hardware 32-bit
// multiplier, notably), so the ESP8266 build keeps the plain C++ path below
// instead of trying to share this asm across both architectures.
// ---------------------------------------------------------------------------

// Sums `count` little-endian uint16_t samples pointed to by `samples`.
// Equivalent to:
//   uint32_t total = 0;
//   for (int i = 0; i < count; i++) total += samples[i];
static uint32_t sumSamplesAsm(const uint16_t* samples, int count) {
    uint32_t total;
    asm volatile(
        "movi %[total], 0           \n\t"  // total = 0
        "beqz %[cnt], 2f            \n\t"  // count == 0: skip the loop entirely
        "1:                         \n\t"
        "l16ui a4, %[ptr], 0        \n\t"  // a4 = *ptr (zero-extended 16-bit load)
        "add %[total], %[total], a4 \n\t"  // total += a4
        "addi %[ptr], %[ptr], 2     \n\t"  // ptr += sizeof(uint16_t)
        "addi %[cnt], %[cnt], -1    \n\t"  // cnt--
        "bnez %[cnt], 1b            \n\t"  // loop while cnt != 0
        "2:                         \n\t"
        : [total] "=&r"(total), [ptr] "+r"(samples), [cnt] "+r"(count)
        :
        : "a4", "memory");
    return total;
}

// Returns floor((clamp(value, lo, hi) - lo) * 100 / (hi - lo)), i.e. `value`
// rescaled onto a 0-100 percent range. Uses the ESP32's hardware 32-bit
// multiplier (MULL) for the multiply, and a hand-rolled 32-bit unsigned
// restoring long division for the divide, since Xtensa has no integer
// divide instruction. Equivalent to:
//   if (value <= lo) return 0;
//   if (value >= hi) return 100;
//   return (value - lo) * 100 / (hi - lo);
static uint32_t scaleToPercentAsm(uint32_t value, uint32_t lo, uint32_t hi) {
    uint32_t result;
    asm volatile(
        // --- clamp value to [lo, hi] ---
        "bltu %[val], %[lo], 2f     \n\t"  // if value < lo, clamp low
        "bltu %[hi], %[val], 3f     \n\t"  // if value > hi, clamp high
        "j 4f                       \n\t"
        "2:                         \n\t"
        "mov %[val], %[lo]          \n\t"
        "j 4f                       \n\t"
        "3:                         \n\t"
        "mov %[val], %[hi]          \n\t"
        "4:                         \n\t"

        // --- numerator = (value - lo) * 100 ---
        "sub a5, %[val], %[lo]      \n\t"
        "movi a6, 100               \n\t"
        "mull a5, a5, a6            \n\t"  // a5 = a5 * 100 (hw 32x32 multiply)

        // --- denominator = hi - lo (guard against a misconfigured 0) ---
        "sub a6, %[hi], %[lo]       \n\t"
        "bnez a6, 5f                \n\t"
        "movi %[res], 0             \n\t"
        "j 9f                       \n\t"
        "5:                         \n\t"

        // --- unsigned restoring long division: a5 / a6 -> %[res] ---
        "movi %[res], 0             \n\t"  // quotient = 0
        "movi a7, 0                 \n\t"  // remainder = 0
        "movi a8, 32                \n\t"  // bit counter
        "6:                         \n\t"
        "slli a7, a7, 1             \n\t"  // remainder <<= 1
        "bbci a5, 31, 7f            \n\t"  // bring in numerator's top bit
        "addi a7, a7, 1             \n\t"
        "7:                         \n\t"
        "slli a5, a5, 1             \n\t"  // numerator <<= 1
        "slli %[res], %[res], 1     \n\t"  // quotient <<= 1
        "bltu a7, a6, 8f            \n\t"  // if remainder < divisor, skip
        "sub a7, a7, a6             \n\t"  // remainder -= divisor
        "addi %[res], %[res], 1     \n\t"  // quotient |= 1
        "8:                         \n\t"
        "addi a8, a8, -1            \n\t"
        "bnez a8, 6b                \n\t"
        "9:                         \n\t"
        : [res] "=&r"(result), [val] "+r"(value)
        : [lo] "r"(lo), [hi] "r"(hi)
        : "a5", "a6", "a7", "a8", "memory");
    return result;
}
#endif  // ESP32

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
      _samples(samples == 0 ? 1 : (samples > kMaxSamples ? kMaxSamples : samples)) {}

void BatteryMonitor::begin() {
#if defined(ESP32)
    // 11dB attenuation gives the ADC roughly a 0-3.3V input range.
    analogSetPinAttenuation(_pin, ADC_11db);
#endif
    pinMode(_pin, INPUT);
}

float BatteryMonitor::readAveragedAdcVoltage() const {
    // Reading the ADC itself is a peripheral access via the SDK and stays in
    // C++; only the summation below is hand-rolled asm on ESP32.
    uint16_t samples[kMaxSamples];
    for (uint8_t i = 0; i < _samples; i++) {
#if defined(ESP32)
        // analogReadMilliVolts() applies the ESP32's factory ADC calibration,
        // which is noticeably more accurate than converting the raw counts
        // ourselves.
        samples[i] = static_cast<uint16_t>(analogReadMilliVolts(_pin));
#else
        // Fallback for cores without a calibrated milli-volt helper: convert
        // the raw ADC counts using the configured reference voltage.
        uint32_t raw = analogRead(_pin);
        samples[i] = static_cast<uint16_t>((raw * _adcRefVoltage * 1000.0f) / 1023.0f);
#endif
        delay(2);
    }

#if defined(ESP32)
    uint32_t total = sumSamplesAsm(samples, _samples);
#else
    uint32_t total = 0;
    for (uint8_t i = 0; i < _samples; i++) total += samples[i];
#endif

    float avgMillivolts = static_cast<float>(total) / _samples;
    return avgMillivolts / 1000.0f;
}

uint8_t BatteryMonitor::voltageToPercent(float voltage) const {
    // A real Li-ion/LiPo discharge curve is non-linear (it sags fastest at
    // the very top and bottom), so this linear mapping across the configured
    // voltage range is an estimate, not a precise gauge.
#if defined(ESP32)
    // Do the clamp/scale in fixed-point millivolts so the ESP32 asm routine
    // above (integer-only, no FPU instructions involved) can do the work.
    uint32_t mv = static_cast<uint32_t>(voltage * 1000.0f + 0.5f);
    uint32_t loMv = static_cast<uint32_t>(_vMin * 1000.0f + 0.5f);
    uint32_t hiMv = static_cast<uint32_t>(_vMax * 1000.0f + 0.5f);
    uint32_t pct = scaleToPercentAsm(mv, loMv, hiMv);
    return static_cast<uint8_t>(pct > 100 ? 100 : pct);
#else
    if (voltage <= _vMin) return 0;
    if (voltage >= _vMax) return 100;
    float pct = (voltage - _vMin) / (_vMax - _vMin) * 100.0f;
    return static_cast<uint8_t>(pct + 0.5f);
#endif
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
