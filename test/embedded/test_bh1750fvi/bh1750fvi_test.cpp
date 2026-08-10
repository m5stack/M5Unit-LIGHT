/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
  UnitTest for UnitBH1750FVI (Unit DLight / Hat DLight)
*/
#include <gtest/gtest.h>
#include <driver/gpio.h>
#include <M5Unified.h>
#include <M5UnitUnified.hpp>
#include <googletest/test_template.hpp>
#include <googletest/test_helper.hpp>
#include <unit/unit_BH1750FVI.hpp>
#include <chrono>
#include <cmath>

using namespace m5::unit::googletest;
using namespace m5::unit;
using namespace m5::unit::bh1750fvi;

namespace {
// SoftwareI2C/cache-miss jitter on NessoN1 considered.
constexpr uint32_t INTERVAL_MARGIN_MS{10U};

uint32_t lower_bound(uint32_t expected)
{
    return expected > INTERVAL_MARGIN_MS ? expected - INTERVAL_MARGIN_MS : 0U;
}
uint32_t upper_bound(uint32_t expected)
{
    return expected + INTERVAL_MARGIN_MS;
}
}  // namespace

class TestBH1750FVI : public I2CComponentTestBase<UnitBH1750FVI> {
protected:
    virtual UnitBH1750FVI* get_instance() override
    {
        auto ptr{new m5::unit::UnitBH1750FVI()};
        if (ptr) {
            auto ccfg{ptr->component_config()};
            ptr->component_config(ccfg);
        }
        return ptr;
    }

#if defined(USING_HAT_DLIGHT)
    virtual bool begin() override
    {
        // HatDLight: NessoN1 uses Wire1, other StickC-family boards use Wire.
        // Drive SCL as OUTPUT before bus init (some boards leave it floating on reset).
        const auto pins = m5::unit::wiring::hatI2CPins();
        gpio_set_direction(static_cast<gpio_num_t>(pins.scl), GPIO_MODE_OUTPUT);
        return m5::unit::wiring::addHatI2C(Units, *unit, unit->component_config().clock) && Units.begin();
    }
#endif
};

// --- basic construction ---
TEST_F(TestBH1750FVI, Instance)
{
    SCOPED_TRACE(ustr);
    EXPECT_NE(unit, nullptr);
    EXPECT_TRUE(unit->inPeriodic());  // default config starts periodic on begin()
}

// --- begin() applies config ---
TEST_F(TestBH1750FVI, BeginAppliesConfig)
{
    SCOPED_TRACE(ustr);
    // Defaults: Resolution::High, mtreg == MTREG_DEFAULT
    EXPECT_EQ(unit->resolution(), Resolution::High);
    EXPECT_EQ(unit->mtreg(), MTREG_DEFAULT);
    EXPECT_EQ(unit->readMTreg(), MTREG_DEFAULT);
}

TEST_F(TestBH1750FVI, BeginAppliesNonDefaultConfig)
{
    SCOPED_TRACE(ustr);
    EXPECT_TRUE(unit->stopPeriodicMeasurement());

    auto cfg           = unit->config();
    cfg.start_periodic = false;
    cfg.resolution     = Resolution::High2;
    cfg.mtreg          = MTREG_MIN;
    unit->config(cfg);
    EXPECT_TRUE(unit->begin());

    EXPECT_FALSE(unit->inPeriodic());  // start_periodic=false reflected
    EXPECT_EQ(unit->resolution(), Resolution::High2);
    EXPECT_EQ(unit->mtreg(), MTREG_MIN);
}

// --- begin() applies non-default config in periodic mode ---
TEST_F(TestBH1750FVI, BeginAppliesNonDefaultPeriodicConfig)
{
    SCOPED_TRACE(ustr);
    EXPECT_TRUE(unit->stopPeriodicMeasurement());

    auto cfg           = unit->config();
    cfg.start_periodic = true;
    cfg.resolution     = Resolution::High2;
    cfg.mtreg          = MTREG_MIN;
    unit->config(cfg);
    EXPECT_TRUE(unit->begin());

    EXPECT_TRUE(unit->inPeriodic());  // start_periodic=true reflected
    EXPECT_EQ(unit->resolution(), Resolution::High2);
    EXPECT_EQ(unit->mtreg(), MTREG_MIN);

    // Verify periodic measurements actually run at the configured interval
    auto result = collect_periodic_measurements(unit.get(), 3);
    EXPECT_FALSE(result.timed_out);
    EXPECT_EQ(result.update_count, 3U);
    EXPECT_GE(result.median(), lower_bound(result.expected_interval));
    EXPECT_LE(result.median(), upper_bound(result.expected_interval));
}

// --- config() round-trip (getter/setter) ---
TEST_F(TestBH1750FVI, ConfigRoundTrip)
{
    SCOPED_TRACE(ustr);

    auto cfg           = unit->config();
    cfg.start_periodic = false;
    cfg.resolution     = Resolution::High2;
    cfg.mtreg          = MTREG_MIN;
    unit->config(cfg);

    auto out = unit->config();
    EXPECT_FALSE(out.start_periodic);
    EXPECT_EQ(out.resolution, Resolution::High2);
    EXPECT_EQ(out.mtreg, MTREG_MIN);
}

// --- MTreg bounds ---
TEST_F(TestBH1750FVI, MTregBounds)
{
    SCOPED_TRACE(ustr);

    EXPECT_TRUE(unit->writeMTreg(MTREG_MIN));
    EXPECT_EQ(unit->mtreg(), MTREG_MIN);

    EXPECT_TRUE(unit->writeMTreg(MTREG_MAX));
    EXPECT_EQ(unit->mtreg(), MTREG_MAX);

    EXPECT_TRUE(unit->writeMTreg(MTREG_DEFAULT));
    EXPECT_EQ(unit->mtreg(), MTREG_DEFAULT);

    // Out of range must reject
    EXPECT_FALSE(unit->writeMTreg(MTREG_MIN - 1));
    EXPECT_FALSE(unit->writeMTreg(MTREG_MAX + 1));
    EXPECT_EQ(unit->mtreg(), MTREG_DEFAULT);  // unchanged after rejection
}

// --- writeMTreg in periodic mode (re-issues mode opcode + apply_interval) ---
TEST_F(TestBH1750FVI, WriteMTregInPeriodic)
{
    SCOPED_TRACE(ustr);
    EXPECT_TRUE(unit->inPeriodic());

    // Default resolution is High; switch mtreg to MIN while running.
    EXPECT_TRUE(unit->writeMTreg(MTREG_MIN));
    EXPECT_EQ(unit->mtreg(), MTREG_MIN);
    EXPECT_TRUE(unit->inPeriodic());

    // After re-issue, the chip should still produce data on the new schedule
    // (High + MTREG_MIN: interval = 180 * 31 / 69 ~= 81 ms).
    auto result = collect_periodic_measurements(unit.get(), 3);
    EXPECT_FALSE(result.timed_out);
    EXPECT_EQ(result.update_count, 3U);
    EXPECT_GE(result.median(), lower_bound(result.expected_interval));
    EXPECT_LE(result.median(), upper_bound(result.expected_interval));
}

// --- SensitivityFactor bounds ---
TEST_F(TestBH1750FVI, SensitivityFactorBounds)
{
    SCOPED_TRACE(ustr);

    EXPECT_TRUE(unit->writeSensitivityFactor(SENSITIVITY_FACTOR_MIN));
    EXPECT_GE(unit->mtreg(), MTREG_MIN);

    EXPECT_TRUE(unit->writeSensitivityFactor(1.0f));
    EXPECT_NEAR(unit->mtreg(), MTREG_DEFAULT, 1);

    EXPECT_TRUE(unit->writeSensitivityFactor(SENSITIVITY_FACTOR_MAX));
    EXPECT_LE(unit->mtreg(), MTREG_MAX);

    // Out of range must reject
    EXPECT_FALSE(unit->writeSensitivityFactor(SENSITIVITY_FACTOR_MIN - 0.1f));
    EXPECT_FALSE(unit->writeSensitivityFactor(SENSITIVITY_FACTOR_MAX + 0.1f));
}

// --- Single-shot measurement ---
TEST_F(TestBH1750FVI, SingleShot)
{
    SCOPED_TRACE(ustr);

    // measureSingleShot refuses while periodic is running
    Data d{};
    EXPECT_FALSE(unit->measureSingleShot(d));

    EXPECT_TRUE(unit->stopPeriodicMeasurement());
    EXPECT_FALSE(unit->inPeriodic());

    EXPECT_TRUE(unit->measureSingleShot(d, Resolution::High, MTREG_DEFAULT));
    EXPECT_EQ(d.resolution, Resolution::High);
    EXPECT_EQ(d.mtreg, MTREG_DEFAULT);

    EXPECT_TRUE(unit->measureSingleShot(d, Resolution::Low, MTREG_MIN));
    EXPECT_EQ(d.resolution, Resolution::Low);
    EXPECT_EQ(d.mtreg, MTREG_MIN);

    EXPECT_TRUE(unit->startPeriodicMeasurement());
    EXPECT_TRUE(unit->inPeriodic());
}

// --- Periodic measurement collection ---
TEST_F(TestBH1750FVI, Periodic)
{
    SCOPED_TRACE(ustr);

    // Reconfigure to a short interval for quick test run.
    EXPECT_TRUE(unit->stopPeriodicMeasurement());
    EXPECT_TRUE(unit->startPeriodicMeasurement(Resolution::Low, MTREG_DEFAULT));
    EXPECT_TRUE(unit->inPeriodic());

    auto result = collect_periodic_measurements(unit.get(), 5);
    EXPECT_FALSE(result.timed_out);
    EXPECT_EQ(result.update_count, 5U);

    EXPECT_GE(result.median(), lower_bound(result.expected_interval));
    EXPECT_LE(result.median(), upper_bound(result.expected_interval));
}

// --- All Resolutions in periodic mode ---
TEST_F(TestBH1750FVI, AllResolutionsPeriodic)
{
    SCOPED_TRACE(ustr);

    const Resolution table[] = {Resolution::Low, Resolution::High, Resolution::High2};
    for (auto res : table) {
        EXPECT_TRUE(unit->stopPeriodicMeasurement());
        EXPECT_TRUE(unit->startPeriodicMeasurement(res, MTREG_DEFAULT));
        EXPECT_EQ(unit->resolution(), res);
        EXPECT_TRUE(unit->inPeriodic());

        auto result = collect_periodic_measurements(unit.get(), 3);
        EXPECT_FALSE(result.timed_out);
        EXPECT_EQ(result.update_count, 3U);
        EXPECT_GE(result.median(), lower_bound(result.expected_interval));
        EXPECT_LE(result.median(), upper_bound(result.expected_interval));
    }
}

// --- Unit-level accessor (lux/raw) returns valid data after periodic ---
TEST_F(TestBH1750FVI, UnitAccessorAfterPeriodic)
{
    SCOPED_TRACE(ustr);

    EXPECT_TRUE(unit->stopPeriodicMeasurement());
    EXPECT_TRUE(unit->startPeriodicMeasurement(Resolution::Low, MTREG_DEFAULT));

    auto result = collect_periodic_measurements(unit.get(), 3);
    EXPECT_FALSE(result.timed_out);
    EXPECT_EQ(result.update_count, 3U);
    EXPECT_GE(result.median(), lower_bound(result.expected_interval));
    EXPECT_LE(result.median(), upper_bound(result.expected_interval));

    EXPECT_FALSE(unit->empty());
    EXPECT_GE(unit->available(), 1U);
    // mtreg=69 always yields a defined lux (never NaN); raw can legitimately be 0 in full dark.
    EXPECT_FALSE(std::isnan(unit->lux()));
    EXPECT_GE(unit->lux(), 0.0f);
}

// --- Power management smoke test ---
TEST_F(TestBH1750FVI, Power)
{
    SCOPED_TRACE(ustr);

    EXPECT_TRUE(unit->stopPeriodicMeasurement());
    EXPECT_TRUE(unit->powerOn());
    EXPECT_TRUE(unit->powerDown());
    EXPECT_TRUE(unit->powerOn());
    EXPECT_TRUE(unit->softReset());
}

// --- powerDown() must clear _periodic to avoid stale read on next update() ---
TEST_F(TestBH1750FVI, PowerDownClearsPeriodic)
{
    SCOPED_TRACE(ustr);
    // Default config starts periodic on begin() → inPeriodic() is true here.
    EXPECT_TRUE(unit->inPeriodic());
    EXPECT_TRUE(unit->powerDown());
    // Contract: powerDown() puts the chip in Power Down AND clears _periodic so
    // subsequent update() won't try to read stale data from an asleep sensor.
    EXPECT_FALSE(unit->inPeriodic());
}

// --- 400 kHz I2C clock is the datasheet ceiling; guard against regressions ---
TEST_F(TestBH1750FVI, ClockIsCappedAt400kHz)
{
    SCOPED_TRACE(ustr);
    EXPECT_EQ(unit->component_config().clock, 400 * 1000U);
}

// --- interval() = ceil(base_ms * mtreg / MTREG_DEFAULT); check against explicit expected values ---
TEST_F(TestBH1750FVI, IntervalMatchesFormula)
{
    SCOPED_TRACE(ustr);
    EXPECT_TRUE(unit->stopPeriodicMeasurement());

    struct Case {
        Resolution res;
        uint8_t mtreg;
        uint32_t expected_ms;
    };
    const Case cases[] = {
        {Resolution::High, MTREG_DEFAULT, 180},
        {Resolution::High2, MTREG_DEFAULT, 180},
        {Resolution::Low, MTREG_DEFAULT, 24},
        {Resolution::High, MTREG_MIN, (180U * MTREG_MIN + MTREG_DEFAULT - 1) / MTREG_DEFAULT},
        {Resolution::High, MTREG_MAX, (180U * MTREG_MAX + MTREG_DEFAULT - 1) / MTREG_DEFAULT},
        {Resolution::Low, MTREG_MIN, (24U * MTREG_MIN + MTREG_DEFAULT - 1) / MTREG_DEFAULT},
        {Resolution::Low, MTREG_MAX, (24U * MTREG_MAX + MTREG_DEFAULT - 1) / MTREG_DEFAULT},
    };
    for (const auto& c : cases) {
        EXPECT_TRUE(unit->startPeriodicMeasurement(c.res, c.mtreg));
        EXPECT_EQ(unit->interval(), c.expected_ms)
            << "res=" << static_cast<int>(c.res) << " mtreg=" << static_cast<unsigned>(c.mtreg);
        EXPECT_TRUE(unit->stopPeriodicMeasurement());
    }
}
