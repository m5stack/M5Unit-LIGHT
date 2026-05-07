/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
  UnitTest for UnitLight (U021, LM393 + photoresistor)
*/
#include <gtest/gtest.h>
#include <M5Unified.h>
#include <M5UnitUnified.hpp>
#include <googletest/test_template.hpp>
#include <googletest/test_helper.hpp>
#include <unit/unit_Light.hpp>
#include <cmath>

using namespace m5::unit::googletest;
using namespace m5::unit;
using namespace m5::unit::light;

namespace {
// Generous tolerance for ADC sampling jitter on ESP32-class targets.
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

class TestLight : public GPIOComponentTestBase<UnitLight> {
protected:
    virtual UnitLight* get_instance() override
    {
        return new m5::unit::UnitLight();
    }
};

// --- pure-API tests (no hardware required) ---
// NOTE: Data::normalized() formula tests live in test/data_test.cpp so they
// run in both native (sdl) and embedded test sessions.

TEST(LightConfig, Defaults)
{
    UnitLight::config_t cfg{};
    EXPECT_TRUE(cfg.start_periodic);
    EXPECT_EQ(cfg.interval_ms, 50u);
    EXPECT_EQ(cfg.dark, 0u);
    EXPECT_EQ(cfg.bright, 4095u);
}

// --- HW-in-loop tests (require GPIO adapter to be wired to U021) ---

TEST_F(TestLight, Instance)
{
    SCOPED_TRACE(ustr);
    EXPECT_NE(unit, nullptr);
    EXPECT_TRUE(unit->inPeriodic());  // default config starts periodic
}

TEST_F(TestLight, BeginAppliesConfig)
{
    SCOPED_TRACE(ustr);
    // Default calibration
    EXPECT_EQ(unit->dark(), 0u);
    EXPECT_EQ(unit->bright(), 4095u);
}

TEST_F(TestLight, BeginAppliesNonDefaultConfig)
{
    SCOPED_TRACE(ustr);
    EXPECT_TRUE(unit->stopPeriodicMeasurement());

    auto cfg           = unit->config();
    cfg.start_periodic = false;
    cfg.interval_ms    = 123;
    cfg.dark           = 100;
    cfg.bright         = 3000;
    unit->config(cfg);
    EXPECT_TRUE(unit->begin());

    EXPECT_FALSE(unit->inPeriodic());
    EXPECT_EQ(unit->dark(), 100u);
    EXPECT_EQ(unit->bright(), 3000u);
}

TEST_F(TestLight, BeginAppliesNonDefaultPeriodicConfig)
{
    SCOPED_TRACE(ustr);
    EXPECT_TRUE(unit->stopPeriodicMeasurement());

    auto cfg           = unit->config();
    cfg.start_periodic = true;
    cfg.interval_ms    = 50;  // short interval for quick verification
    cfg.dark           = 100;
    cfg.bright         = 3000;
    unit->config(cfg);
    EXPECT_TRUE(unit->begin());

    EXPECT_TRUE(unit->inPeriodic());
    EXPECT_EQ(unit->dark(), 100u);
    EXPECT_EQ(unit->bright(), 3000u);

    auto result = collect_periodic_measurements(unit.get(), 3);
    EXPECT_FALSE(result.timed_out);
    EXPECT_EQ(result.update_count, 3U);
    EXPECT_GE(result.median(), lower_bound(result.expected_interval));
    EXPECT_LE(result.median(), upper_bound(result.expected_interval));

    // Restore default calibration so subsequent tests are not affected
    unit->resetCalibration();
}

TEST_F(TestLight, ConfigRoundTrip)
{
    SCOPED_TRACE(ustr);

    auto cfg           = unit->config();
    cfg.start_periodic = false;
    cfg.interval_ms    = 123;
    cfg.dark           = 100;
    cfg.bright         = 3000;
    unit->config(cfg);

    auto out = unit->config();
    EXPECT_FALSE(out.start_periodic);
    EXPECT_EQ(out.interval_ms, 123u);
    EXPECT_EQ(out.dark, 100u);
    EXPECT_EQ(out.bright, 3000u);
}

TEST_F(TestLight, SetCalibrationAndReset)
{
    SCOPED_TRACE(ustr);

    unit->setCalibration(500, 3800);
    EXPECT_EQ(unit->dark(), 500u);
    EXPECT_EQ(unit->bright(), 3800u);

    unit->resetCalibration();
    EXPECT_EQ(unit->dark(), 0u);
    EXPECT_EQ(unit->bright(), 4095u);
}

TEST_F(TestLight, CalibrateLivePath)
{
    SCOPED_TRACE(ustr);

    // Reset to known initial state (dark=0, bright=4095)
    unit->resetCalibration();

    EXPECT_TRUE(unit->calibrateDark());
    EXPECT_TRUE(unit->calibrateBright());

    // ADC range check (12-bit). Under stable ambient light, dark == bright is legitimate.
    EXPECT_LE(unit->dark(), 4095u);
    EXPECT_LE(unit->bright(), 4095u);
}

TEST_F(TestLight, Periodic)
{
    SCOPED_TRACE(ustr);
    EXPECT_TRUE(unit->inPeriodic());

    auto result = collect_periodic_measurements(unit.get(), 5);
    EXPECT_FALSE(result.timed_out);
    EXPECT_EQ(result.update_count, 5U);
    EXPECT_GE(result.median(), lower_bound(result.expected_interval));
    EXPECT_LE(result.median(), upper_bound(result.expected_interval));
}

// --- Unit-level accessor (analog/digital/normalized) returns valid data after periodic ---
TEST_F(TestLight, UnitAccessorAfterPeriodic)
{
    SCOPED_TRACE(ustr);

    auto result = collect_periodic_measurements(unit.get(), 3);
    EXPECT_FALSE(result.timed_out);
    EXPECT_EQ(result.update_count, 3U);
    EXPECT_GE(result.median(), lower_bound(result.expected_interval));
    EXPECT_LE(result.median(), upper_bound(result.expected_interval));

    EXPECT_FALSE(unit->empty());
    EXPECT_GE(unit->available(), 1U);

    // ADC raw is bounded by 12-bit range (0..4095) on supported boards.
    EXPECT_LE(unit->analog(), 4095U);

    // digital() returns either bool value; just verify the call is valid.
    (void)unit->digital();

    // With default calibration (dark=0, bright=4095) span is non-zero, so normalized never NaN.
    const float n{unit->normalized()};
    EXPECT_FALSE(std::isnan(n));
    EXPECT_GE(n, 0.0f);
    EXPECT_LE(n, 100.0f);
}

// --- normalized() respects setCalibration() values ---
TEST_F(TestLight, NormalizedAfterCalibration)
{
    SCOPED_TRACE(ustr);

    unit->setCalibration(0, 4095);
    auto result = collect_periodic_measurements(unit.get(), 3);
    EXPECT_FALSE(result.timed_out);
    EXPECT_EQ(result.update_count, 3U);
    EXPECT_GE(result.median(), lower_bound(result.expected_interval));
    EXPECT_LE(result.median(), upper_bound(result.expected_interval));

    const float n{unit->normalized()};
    EXPECT_FALSE(std::isnan(n));
    EXPECT_GE(n, 0.0f);
    EXPECT_LE(n, 100.0f);
}

TEST_F(TestLight, StopStartPeriodic)
{
    SCOPED_TRACE(ustr);

    EXPECT_TRUE(unit->stopPeriodicMeasurement());
    EXPECT_FALSE(unit->inPeriodic());

    EXPECT_TRUE(unit->startPeriodicMeasurement(20));
    EXPECT_TRUE(unit->inPeriodic());

    auto result = collect_periodic_measurements(unit.get(), 3);
    EXPECT_FALSE(result.timed_out);
    EXPECT_EQ(result.update_count, 3U);
    EXPECT_GE(result.median(), lower_bound(result.expected_interval));
    EXPECT_LE(result.median(), upper_bound(result.expected_interval));
}
