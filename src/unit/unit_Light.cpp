/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*!
  @file unit_Light.cpp
  @brief UnitLight (SKU:U021) for M5UnitUnified
*/
#include "unit_Light.hpp"
#include <M5Utility.hpp>

using namespace m5::utility::mmh3;
using namespace m5::unit::types;
using namespace m5::unit::light;

namespace m5 {
namespace unit {

namespace light {

float Data::normalized() const
{
    // dark / bright are the ADC readings captured while the sensor was covered (dark) and
    // exposed to a bright reference (bright). They DO NOT imply dark < bright: on U021
    // the photoresistor circuit polarity is board-specific (some revisions yield higher
    // ADC in the dark, others in the light), so dark > bright is legitimate. The mapping
    // always emits 0% at the dark reference and 100% at the bright reference regardless
    // of which endpoint is the larger ADC number.
    if (dark == bright) {
        return std::numeric_limits<float>::quiet_NaN();
    }
    const float span{static_cast<float>(bright) - static_cast<float>(dark)};
    float v{(static_cast<float>(analog_raw) - static_cast<float>(dark)) * 100.0f / span};
    if (v < 0.0f) v = 0.0f;
    if (v > 100.0f) v = 100.0f;
    return v;
}

}  // namespace light

// class UnitLight
const char UnitLight::name[] = "UnitLight";
const types::uid_t UnitLight::uid{"UnitLight"_mmh3};
const types::attr_t UnitLight::attr{attribute::AccessGPIO};

bool UnitLight::begin()
{
    auto ssize{stored_size()};
    assert(ssize && "stored_size must be greater than zero");
    if (ssize != _data->capacity()) {
        _data.reset(new m5::container::CircularBuffer<light::Data>(ssize));
        if (!_data) {
            M5_LIB_LOGE("Failed to allocate");
            return false;
        }
    }

    auto ad{adapter()};
    if (!ad) {
        M5_LIB_LOGE("Adapter is null");
        return false;
    }

    // RX receives the photoresistor analog voltage; TX receives the comparator digital output.
    if (!pinModeRX(gpio::Mode::Analog)) {
        M5_LIB_LOGE("Failed to set RX to Analog");
        return false;
    }
    if (!pinModeTX(gpio::Mode::Input)) {
        M5_LIB_LOGE("Failed to set TX to Input");
        return false;
    }

    _dark   = _cfg.dark;
    _bright = _cfg.bright;

    return _cfg.start_periodic ? start_periodic_measurement(_cfg.interval_ms) : true;
}

void UnitLight::update(const bool force)
{
    _updated = false;
    if (inPeriodic()) {
        elapsed_time_t at{m5::utility::millis()};
        if (force || !_latest || (at - _latest) >= _interval) {
            light::Data d{};
            if (read_measurement(d)) {
                _updated = true;
                _latest  = at;
                _data->push_back(d);
            }
        }
    }
}

bool UnitLight::calibrateDark()
{
    uint16_t v{};
    if (!readAnalogRX(v)) {
        return false;
    }
    _dark = v;
    return true;
}

bool UnitLight::calibrateBright()
{
    uint16_t v{};
    if (!readAnalogRX(v)) {
        return false;
    }
    _bright = v;
    return true;
}

void UnitLight::setCalibration(const uint16_t dark, const uint16_t bright)
{
    _dark   = dark;
    _bright = bright;
}

void UnitLight::resetCalibration()
{
    _dark   = 0;
    _bright = 4095;
}

bool UnitLight::start_periodic_measurement()
{
    return start_periodic_measurement(_cfg.interval_ms);
}

bool UnitLight::start_periodic_measurement(const uint32_t interval_ms)
{
    if (inPeriodic()) {
        M5_LIB_LOGD("Already running");
        return false;
    }
    _interval = interval_ms;
    _periodic = true;
    _latest   = m5::utility::millis();
    return true;
}

bool UnitLight::stop_periodic_measurement()
{
    _periodic = false;
    return true;
}

bool UnitLight::read_measurement(light::Data& data)
{
    uint16_t a{};
    bool d{};
    if (!readAnalogRX(a)) {
        return false;
    }
    if (!readDigitalTX(d)) {
        return false;
    }
    data.analog_raw = a;
    data.digital    = d;
    data.dark       = _dark;
    data.bright     = _bright;
    return true;
}

}  // namespace unit
}  // namespace m5
