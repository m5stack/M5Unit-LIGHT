/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*!
  @file unit_BH1750FVI.cpp
  @brief BH1750FVI Unit for M5UnitUnified
*/
#include "unit_BH1750FVI.hpp"
#include <M5Utility.hpp>
#include <cmath>

using namespace m5::utility::mmh3;
using namespace m5::unit::types;
using namespace m5::unit::bh1750fvi;
using namespace m5::unit::bh1750fvi::command;

namespace {

// Base measurement time in milliseconds (MAX values from datasheet; use max for safety)
constexpr uint32_t BASE_MS_LOW{24};
constexpr uint32_t BASE_MS_HIGH{180};

// Number of retries for the first transaction on SoftwareI2C (NessoN1 workaround)
constexpr uint8_t FIRST_TRANSACTION_RETRIES{3};
constexpr uint32_t FIRST_TRANSACTION_RETRY_DELAY_MS{100};

// Guard delay applied inside write_opcode() after every successful opcode write.
// The BH1750 datasheet requires a STOP between opcodes (which we already emit) but
// the chip also needs a few ms of settle time before it will honor the next opcode —
// without this, the Continuous-mode opcode can be absorbed as a One-Time command
// and the chip returns the same reading forever. Keeping the delay inside write_opcode()
// means every caller is safe by construction; no site needs to remember to add it.
constexpr uint32_t OPCODE_GUARD_DELAY_MS{5};

}  // namespace

namespace m5 {
namespace unit {

namespace bh1750fvi {

float Data::lux() const
{
    // lux = raw / 1.2 * (69 / mtreg) [ / 2 if High2 ]
    if (mtreg == 0) {
        return std::numeric_limits<float>::quiet_NaN();
    }
    const float base{static_cast<float>(raw) / 1.2f};
    const float scale{static_cast<float>(MTREG_DEFAULT) / static_cast<float>(mtreg)};
    float v{base * scale};
    if (resolution == Resolution::High2) {
        v *= 0.5f;
    }
    return v;
}

}  // namespace bh1750fvi

// class UnitBH1750FVI
const char UnitBH1750FVI::name[] = "UnitBH1750FVI";
const types::uid_t UnitBH1750FVI::uid{"UnitBH1750FVI"_mmh3};
const types::attr_t UnitBH1750FVI::attr{attribute::AccessI2C};

bool UnitBH1750FVI::begin()
{
    auto ssize{stored_size()};
    assert(ssize && "stored_size must be greater than zero");
    if (ssize != _data->capacity()) {
        _data.reset(new m5::container::CircularBuffer<bh1750fvi::Data>(ssize));
        if (!_data) {
            M5_LIB_LOGE("Failed to allocate");
            return false;
        }
    }

    m5::utility::delay(50);  // Allow BH1750 VCC rails to settle after Wire.begin().

    // SoftwareI2C first-transaction retry (NessoN1 workaround).
    // Larger binaries can cause the first transaction to NACK due to Flash instruction cache timing.
    bool awake{false};
    for (uint8_t i = 0; i < FIRST_TRANSACTION_RETRIES; ++i) {
        if (powerOn()) {
            awake = true;
            break;
        }
        m5::utility::delay(FIRST_TRANSACTION_RETRY_DELAY_MS);
    }
    if (!awake) {
        M5_LIB_LOGE("Failed to power on after retries");
        return false;
    }

    if (!softReset()) {
        M5_LIB_LOGE("Failed to reset");
        return false;
    }

    if (_cfg.mtreg < MTREG_MIN || _cfg.mtreg > MTREG_MAX) {
        M5_LIB_LOGE("Invalid mtreg %u", _cfg.mtreg);
        return false;
    }

    // Apply MTreg from config (default 69 is the chip default, but set explicitly for consistency)
    if (_cfg.mtreg != MTREG_DEFAULT) {
        if (!write_mtreg_opcodes(_cfg.mtreg)) {
            M5_LIB_LOGE("Failed to write MTreg");
            return false;
        }
    }
    _mtreg      = _cfg.mtreg;
    _resolution = _cfg.resolution;

    return _cfg.start_periodic ? start_periodic_measurement(_cfg.resolution, _cfg.mtreg) : true;
}

void UnitBH1750FVI::update(const bool force)
{
    _updated = false;
    if (inPeriodic()) {
        elapsed_time_t at{m5::utility::millis()};
        if (force || !_latest || (at - _latest) >= _interval) {
            bh1750fvi::Data d{};
            if (read_measurement(d)) {
                _updated = true;
                _latest  = at;
                _data->push_back(d);
            }
        }
    }
}

bool UnitBH1750FVI::measureSingleShot(bh1750fvi::Data& data, const bh1750fvi::Resolution resolution,
                                      const uint8_t mtreg)
{
    if (inPeriodic()) {
        M5_LIB_LOGD("Periodic measurements are running");
        return false;
    }
    if (mtreg < MTREG_MIN || mtreg > MTREG_MAX) {
        M5_LIB_LOGE("Invalid mtreg %u", mtreg);
        return false;
    }

    if (mtreg != _mtreg) {
        if (!write_mtreg_opcodes(mtreg)) {
            return false;
        }
        _mtreg = mtreg;
    }

    if (!write_opcode(opcode_for(Mode::OneTime, resolution))) {
        return false;
    }

    const uint32_t base_ms{(resolution == Resolution::Low) ? BASE_MS_LOW : BASE_MS_HIGH};
    const uint32_t wait_ms{(base_ms * mtreg + MTREG_DEFAULT - 1U) / MTREG_DEFAULT};
    m5::utility::delay(wait_ms);

    // read_measurement() overwrites data.mtreg/resolution with the periodic-mode cache;
    // for single-shot we want the requested values to be reflected in `data`.
    if (!read_measurement(data)) {
        return false;
    }
    data.mtreg      = mtreg;
    data.resolution = resolution;
    return true;
}

bool UnitBH1750FVI::writeMTreg(const uint8_t mtreg)
{
    if (mtreg < MTREG_MIN || mtreg > MTREG_MAX) {
        M5_LIB_LOGE("Invalid mtreg %u", mtreg);
        return false;
    }
    if (!write_mtreg_opcodes(mtreg)) {
        return false;
    }

    if (inPeriodic()) {
        // Re-send the current mode opcode so the sensor picks up the new MTreg
        if (!write_opcode(opcode_for(Mode::Continuous, _resolution))) {
            return false;
        }
    }
    // Commit cache only after all opcode writes succeed.
    _mtreg = mtreg;
    if (inPeriodic()) {
        apply_interval(_resolution, _mtreg);
        _latest = m5::utility::millis();
    }
    return true;
}

bool UnitBH1750FVI::writeSensitivityFactor(const float factor)
{
    if (!(factor >= SENSITIVITY_FACTOR_MIN && factor <= SENSITIVITY_FACTOR_MAX)) {
        M5_LIB_LOGE("Invalid factor %f", factor);
        return false;
    }
    const float mt_f{static_cast<float>(MTREG_DEFAULT) * factor};
    long mt{std::lround(mt_f)};
    if (mt < MTREG_MIN) mt = MTREG_MIN;
    if (mt > MTREG_MAX) mt = MTREG_MAX;
    return writeMTreg(static_cast<uint8_t>(mt));
}

bool UnitBH1750FVI::powerOn()
{
    return write_opcode(POWER_ON);
}

bool UnitBH1750FVI::powerDown()
{
    return write_opcode(POWER_DOWN);
}

bool UnitBH1750FVI::softReset()
{
    return write_opcode(RESET);
}

bool UnitBH1750FVI::start_periodic_measurement()
{
    return start_periodic_measurement(_cfg.resolution, _cfg.mtreg);
}

bool UnitBH1750FVI::start_periodic_measurement(const bh1750fvi::Resolution resolution, const uint8_t mtreg)
{
    if (inPeriodic()) {
        M5_LIB_LOGD("Already running");
        return false;
    }
    if (mtreg < MTREG_MIN || mtreg > MTREG_MAX) {
        M5_LIB_LOGE("Invalid mtreg %u", mtreg);
        return false;
    }

    // Wake from Power Down (stop_periodic_measurement leaves the chip in Power Down).
    if (!powerOn()) {
        return false;
    }

    if (mtreg != _mtreg) {
        if (!write_mtreg_opcodes(mtreg)) {
            return false;
        }
        _mtreg = mtreg;
    }

    if (!write_opcode(opcode_for(Mode::Continuous, resolution))) {
        return false;
    }
    _resolution = resolution;
    _periodic   = true;
    apply_interval(resolution, mtreg);
    // Wait for the first conversion to complete so the chip settles into Continuous mode
    // and the data register holds a fresh reading before the first read_measurement().
    m5::utility::delay(_interval + 20);
    _latest = m5::utility::millis();
    return true;
}

bool UnitBH1750FVI::stop_periodic_measurement()
{
    // Stop by entering Power Down. The next startPeriodicMeasurement() must re-issue PowerOn.
    if (!powerDown()) {
        return false;
    }
    _periodic = false;
    return true;
}

bool UnitBH1750FVI::read_measurement(bh1750fvi::Data& data)
{
    uint8_t buf[2]{};
    if (readWithTransaction(buf, 2) != m5::hal::error::error_t::OK) {
        return false;
    }
    data.raw        = (static_cast<uint16_t>(buf[0]) << 8) | buf[1];
    data.mtreg      = _mtreg;
    data.resolution = _resolution;
    // M5_LIB_LOGI("read: 0x%02X 0x%02X raw=%u", buf[0], buf[1], data.raw);
    return true;
}

bool UnitBH1750FVI::write_opcode(const uint8_t opcode)
{
    // BH1750 accepts exactly one opcode per I2C transaction (with STOP), followed by a
    // short guard delay so the chip settles before the next opcode. See OPCODE_GUARD_DELAY_MS.
    uint8_t op{opcode};
    if (writeWithTransaction(&op, 1) != m5::hal::error::error_t::OK) {
        return false;
    }
    m5::utility::delay(OPCODE_GUARD_DELAY_MS);
    return true;
}

bool UnitBH1750FVI::write_mtreg_opcodes(const uint8_t mtreg)
{
    // High bits: 0x40 | ((mtreg >> 5) & 0x07)
    // Low  bits: 0x60 | (mtreg & 0x1F)
    // Each must be its own I2C transaction; write_opcode() already inserts the guard delay
    // after each transaction, so high→low is safely separated without an explicit delay here.
    const uint8_t high{static_cast<uint8_t>(CHANGE_MEASUREMENT_TIME_HIGH | ((mtreg >> 5) & 0x07))};
    const uint8_t low{static_cast<uint8_t>(CHANGE_MEASUREMENT_TIME_LOW | (mtreg & 0x1F))};
    if (!write_opcode(high)) {
        return false;
    }
    return write_opcode(low);
}

uint8_t UnitBH1750FVI::opcode_for(const bh1750fvi::Mode mode, const bh1750fvi::Resolution resolution) const
{
    if (mode == Mode::Continuous) {
        switch (resolution) {
            case Resolution::Low:
                return CONTINUOUS_L_RES_MODE;
            case Resolution::High:
                return CONTINUOUS_H_RES_MODE;
            case Resolution::High2:
                return CONTINUOUS_H_RES_MODE2;
        }
    } else {
        switch (resolution) {
            case Resolution::Low:
                return ONE_TIME_L_RES_MODE;
            case Resolution::High:
                return ONE_TIME_H_RES_MODE;
            case Resolution::High2:
                return ONE_TIME_H_RES_MODE2;
        }
    }
    return CONTINUOUS_H_RES_MODE;  // fallback
}

void UnitBH1750FVI::apply_interval(const bh1750fvi::Resolution resolution, const uint8_t mtreg)
{
    const uint32_t base_ms{(resolution == Resolution::Low) ? BASE_MS_LOW : BASE_MS_HIGH};
    _interval = (base_ms * mtreg + MTREG_DEFAULT - 1U) / MTREG_DEFAULT;
    M5_LIB_LOGD("interval %lu ms (res=%u mtreg=%u)", static_cast<unsigned long>(_interval),
                static_cast<unsigned>(resolution), mtreg);
}

}  // namespace unit
}  // namespace m5
