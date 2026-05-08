/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
  Pure-formula tests for Data structs (no hardware required).
  Placed at test/ root so it is compiled for both embedded and native (sdl) test runs.
*/
#include <gtest/gtest.h>
#include <unit/unit_BH1750FVI.hpp>
#include <unit/unit_Light.hpp>
#include <cmath>

// ---------------------------------------------------------------------------
// bh1750fvi::Data::lux()
//   lux = raw / 1.2 * (69 / mtreg) [/ 2 if High2]
//   Returns NaN when mtreg == 0.
// ---------------------------------------------------------------------------
namespace {
using m5::unit::bh1750fvi::Data;
using m5::unit::bh1750fvi::MTREG_DEFAULT;
using m5::unit::bh1750fvi::MTREG_MAX;
using m5::unit::bh1750fvi::MTREG_MIN;
using m5::unit::bh1750fvi::Resolution;
}  // namespace

TEST(BH1750FVIData, LuxFormulaHigh)
{
    // raw=1200, mtreg=69 (default), Resolution::High -> 1000.0 lx
    Data d;
    d.raw        = 1200;
    d.mtreg      = MTREG_DEFAULT;
    d.resolution = Resolution::High;
    EXPECT_NEAR(d.lux(), 1000.0f, 0.1f);
}

TEST(BH1750FVIData, LuxFormulaHigh2HalvesValue)
{
    Data d;
    d.raw        = 1200;
    d.mtreg      = MTREG_DEFAULT;
    d.resolution = Resolution::High2;
    EXPECT_NEAR(d.lux(), 500.0f, 0.1f);
}

TEST(BH1750FVIData, LuxFormulaLow)
{
    // Low resolution uses the same formula as High (no /2 factor).
    Data d;
    d.raw        = 1200;
    d.mtreg      = MTREG_DEFAULT;
    d.resolution = Resolution::Low;
    EXPECT_NEAR(d.lux(), 1000.0f, 0.1f);
}

TEST(BH1750FVIData, LuxMTregScaling)
{
    Data d;
    d.raw        = 1200;
    d.resolution = Resolution::High;

    d.mtreg = MTREG_DEFAULT * 2;  // 138
    EXPECT_NEAR(d.lux(), 500.0f, 0.1f);

    d.mtreg = MTREG_MIN;  // 31
    EXPECT_NEAR(d.lux(), 1000.0f * 69.0f / 31.0f, 0.01f);

    d.mtreg = MTREG_MAX;  // 254
    EXPECT_NEAR(d.lux(), 1000.0f * 69.0f / 254.0f, 0.01f);
}

TEST(BH1750FVIData, LuxMTregZeroReturnsNaN)
{
    Data d;
    d.raw        = 1200;
    d.mtreg      = 0;
    d.resolution = Resolution::High;
    EXPECT_TRUE(std::isnan(d.lux()));
}

TEST(BH1750FVIData, LuxRawZeroIsZero)
{
    Data d;
    d.raw        = 0;
    d.mtreg      = MTREG_DEFAULT;
    d.resolution = Resolution::High;
    EXPECT_FLOAT_EQ(d.lux(), 0.0f);
}

// ---------------------------------------------------------------------------
// light::Data::normalized()
//   Maps analog_raw to 0..100 (%), 0% at dark and 100% at bright.
//   dark/bright polarity is free (dark > bright is legitimate on U021).
//   Returns NaN when dark == bright.
// ---------------------------------------------------------------------------
namespace {
using LightData = m5::unit::light::Data;
}  // namespace

TEST(LightData, NormalizedNormalPolarity)
{
    LightData d;
    d.dark   = 0;
    d.bright = 4095;

    d.analog_raw = 0;
    EXPECT_FLOAT_EQ(d.normalized(), 0.0f);

    d.analog_raw = 4095;
    EXPECT_FLOAT_EQ(d.normalized(), 100.0f);

    d.analog_raw = 2048;
    EXPECT_NEAR(d.normalized(), 100.0f * 2048.0f / 4095.0f, 0.01f);
}

TEST(LightData, NormalizedReversedPolarity)
{
    // U021-style polarity: dark ADC > bright ADC.
    LightData d;
    d.dark   = 4095;
    d.bright = 0;

    d.analog_raw = 4095;
    EXPECT_FLOAT_EQ(d.normalized(), 0.0f);

    d.analog_raw = 0;
    EXPECT_FLOAT_EQ(d.normalized(), 100.0f);

    d.analog_raw = 2000;
    EXPECT_NEAR(d.normalized(), 100.0f * (4095.0f - 2000.0f) / 4095.0f, 0.01f);
}

TEST(LightData, NormalizedClampToRange)
{
    LightData d;
    d.dark   = 100;
    d.bright = 4000;

    d.analog_raw = 50;  // below dark reference
    EXPECT_FLOAT_EQ(d.normalized(), 0.0f);

    d.analog_raw = 4095;  // above bright reference
    EXPECT_FLOAT_EQ(d.normalized(), 100.0f);
}

TEST(LightData, NormalizedClampToRangeReversed)
{
    LightData d;
    d.dark   = 4000;
    d.bright = 100;

    d.analog_raw = 4095;  // outside dark side
    EXPECT_FLOAT_EQ(d.normalized(), 0.0f);

    d.analog_raw = 50;  // outside bright side
    EXPECT_FLOAT_EQ(d.normalized(), 100.0f);
}

TEST(LightData, NormalizedEqualEndpointsReturnsNaN)
{
    LightData d;
    d.dark       = 2048;
    d.bright     = 2048;
    d.analog_raw = 2048;
    EXPECT_TRUE(std::isnan(d.normalized()));
}
