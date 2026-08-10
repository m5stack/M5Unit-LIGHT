/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
  Example using M5UnitUnified for UnitDLight / HatDLight (BH1750FVI)

  BtnA click: toggle Periodic <-> SingleShot
              - Periodic:   stop and take a single-shot reading
              - SingleShot: resume Periodic measurement
  BtnA hold:  cycle resolution Low -> High -> High2 (also re-measures
              for instant feedback in either mode)
*/
#include <driver/gpio.h>
#include <M5Unified.h>
#include <M5UnitUnified.h>
#include <M5UnitUnifiedLIGHT.h>
#include <wiring/m5_unit_unified_wiring.hpp>  // Board-aware connection helpers (include last)
#include <cmath>                              // std::isnan
#include <limits>                             // std::numeric_limits

// *************************************************************
// Choose one define symbol to match the unit you are using
// *************************************************************
#if !defined(USING_UNIT_DLIGHT) && !defined(USING_HAT_DLIGHT)
// For UnitDLight (U136)
// #define USING_UNIT_DLIGHT
// For HatDLight (U134)
// #define USING_HAT_DLIGHT
#endif

#if defined(USING_UNIT_DLIGHT)
#pragma message "Using UnitDLight"
#elif defined(USING_HAT_DLIGHT)
#pragma message "Using HatDLight"
#else
#error Please choose unit!
#endif

using namespace m5::unit::bh1750fvi;

namespace {
auto& lcd = M5.Display;
M5Canvas canvas(&lcd);  // off-screen buffer to prevent flicker

m5::unit::UnitUnified Units;
#if defined(USING_UNIT_DLIGHT)
m5::unit::UnitDLight unit;
#elif defined(USING_HAT_DLIGHT)
m5::unit::HatDLight unit;
#endif

constexpr uint32_t DRAW_INTERVAL_MS{50};
uint32_t last_draw_ms{0};
float last_singleshot_lux{std::numeric_limits<float>::quiet_NaN()};
uint16_t last_singleshot_raw{0};
Resolution current_resolution{Resolution::Low};  // matches setup() config

const char* resolution_label(const Resolution r)
{
    switch (r) {
        case Resolution::Low:
            return "Low";
        case Resolution::High:
            return "High";
        case Resolution::High2:
            return "High2";
    }
    return "?";
}

Resolution cycle_resolution(const Resolution r)
{
    switch (r) {
        case Resolution::Low:
            return Resolution::High;
        case Resolution::High:
            return Resolution::High2;
        case Resolution::High2:
            return Resolution::Low;
    }
    return Resolution::Low;
}

void draw_status()
{
    // Skip drawing on boards without a display (Atom / AtomLite / StampS3 / NanoC6 etc.)
    if (lcd.width() <= 0 || lcd.height() <= 0) {
        return;
    }
    if (canvas.width() <= 0 || canvas.height() <= 0) {
        return;
    }
    const bool wide{canvas.width() >= 240};

    canvas.fillScreen(TFT_BLACK);
    canvas.setCursor(0, 0);
    canvas.setTextColor(TFT_WHITE, TFT_BLACK);
    canvas.setTextSize(1);

    const bool periodic{unit.inPeriodic()};

    if (wide) {
#if defined(USING_UNIT_DLIGHT)
        canvas.printf("Unit DLight\n\n");
#else
        canvas.printf("Hat DLight\n\n");
#endif
        // Main reading emphasized.
        canvas.setTextSize(2);
        if (periodic) {
            canvas.printf("lux: %.1f\n", unit.lux());
            canvas.printf("raw: %u\n", unit.raw());
        } else if (!std::isnan(last_singleshot_lux)) {
            canvas.printf("lux: %.1f\n", last_singleshot_lux);
            canvas.printf("raw: %u\n", last_singleshot_raw);
        } else {
            canvas.printf("lux: -\n");
            canvas.printf("raw: -\n");
        }

        canvas.setTextSize(1);
        canvas.printf("\nmode:       %s\n", periodic ? "Periodic" : "SingleShot");
        canvas.printf("resolution: %s\n", resolution_label(current_resolution));
        canvas.printf("mtreg:      %u\n", unit.mtreg());
        canvas.printf("\nBtnA click: shot/resume\nBtnA hold:  res cycle\n");
    } else {
        // Compact layout for small screens (StickCPlus2 240x135 / AtomS3 128x128 etc.)
#if defined(USING_UNIT_DLIGHT)
        canvas.printf("DLight\n");
#else
        canvas.printf("HatDLight\n");
#endif
        canvas.setTextSize(2);
        if (periodic) {
            canvas.printf("lx:%.0f\n", unit.lux());
            canvas.printf("rw:%u\n", unit.raw());
        } else if (!std::isnan(last_singleshot_lux)) {
            canvas.printf("lx:%.0f\n", last_singleshot_lux);
            canvas.printf("rw:%u\n", last_singleshot_raw);
        } else {
            canvas.printf("lx:-\n");
            canvas.printf("rw:-\n");
        }

        canvas.setTextSize(1);
        canvas.printf("\nm:%s\n", periodic ? "Periodic" : "SingleShot");
        canvas.printf("res:%s mt:%u\n", resolution_label(current_resolution), unit.mtreg());
        canvas.printf("A:shot/resume A^:res\n");
    }

    canvas.pushSprite(0, 0);
}

}  // namespace

void setup()
{
    M5.begin();
    M5.setTouchButtonHeightByRatio(100);

    if (lcd.height() > lcd.width()) {
        lcd.setRotation(1);
    }

    // Prioritize response speed over resolution so brief lighting changes
    // (e.g. a flash) are tracked in near-real-time.
    // Low-res + MTREG_MIN = 24 * 31 / 69 ~= 11ms per sample (4 lx resolution).
    {
        auto cfg       = unit.config();
        cfg.resolution = Resolution::Low;
        cfg.mtreg      = MTREG_MIN;
        unit.config(cfg);
        current_resolution = cfg.resolution;
    }

#if defined(USING_HAT_DLIGHT)
    // HatDLight: NessoN1 uses Wire1, other StickC-family boards use Wire.
    // Drive SCL as OUTPUT before bus init (some boards leave it floating on reset).
    const auto pins = m5::unit::wiring::hatI2CPins();
    gpio_set_direction(static_cast<gpio_num_t>(pins.scl), GPIO_MODE_OUTPUT);
    if (!m5::unit::wiring::addHatI2C(Units, unit, 400 * 1000U) || !Units.begin()) {
        M5_LOGE("Failed to begin");
        m5::unit::wiring::failStop();
    }
#else
    // UnitDLight: NessoN1 -> SoftwareI2C (M5HAL), NanoC6 / NanoH2 -> M5.Ex_I2C, others -> Wire
    if (!m5::unit::wiring::addI2C(Units, unit, 400 * 1000U) || !Units.begin()) {
        M5_LOGE("Failed to begin");
        m5::unit::wiring::failStop();
    }
#endif

    M5_LOGI("M5UnitUnified initialized");
    M5_LOGI("%s", Units.debugInfo().c_str());

    // Prepare off-screen canvas: 1-bit depth, no PSRAM — keeps flicker and memory footprint low.
    if (lcd.width() > 0 && lcd.height() > 0) {
        canvas.setPsram(false);
        canvas.setColorDepth(1);
        canvas.setPaletteColor(0, TFT_BLACK);
        canvas.setPaletteColor(1, TFT_WHITE);
        if (!canvas.createSprite(lcd.width(), lcd.height())) {
            M5_LOGE("Failed to create canvas sprite");
        }
    }
    lcd.fillScreen(TFT_BLACK);
    draw_status();
}

void loop()
{
    M5.update();
    Units.update();

    bool need_redraw{false};

    if (unit.updated()) {
        M5.Log.printf(">lux:%.2f\n>raw:%u\n", unit.lux(), unit.raw());
        // Throttle LCD redraws to avoid flicker from periodic measurements.
        const uint32_t now{m5::utility::millis()};
        if (now - last_draw_ms >= DRAW_INTERVAL_MS) {
            last_draw_ms = now;
            need_redraw  = true;
        }
    }

    // BtnA click: toggle Periodic <-> SingleShot
    //   - Periodic   -> stop, take one shot, stay in SingleShot mode
    //   - SingleShot -> resume Periodic at current_resolution
    if (M5.BtnA.wasClicked()) {
        if (unit.inPeriodic()) {
            unit.stopPeriodicMeasurement();
            Data d{};
            if (unit.measureSingleShot(d, current_resolution, unit.mtreg())) {
                last_singleshot_lux = d.lux();
                last_singleshot_raw = d.raw;
                M5.Log.printf(">singleshot_lux:%.2f\n>singleshot_raw:%u\n", d.lux(), d.raw);
            }
        } else {
            unit.startPeriodicMeasurement(current_resolution, unit.mtreg());
            M5_LOGI("Resumed periodic");
        }
        need_redraw = true;
    }

    // BtnA hold: cycle Low -> High -> High2 (always re-measure to give visible feedback)
    if (M5.BtnA.wasHold()) {
        current_resolution = cycle_resolution(current_resolution);
        M5_LOGI("Resolution -> %s", resolution_label(current_resolution));
        if (unit.inPeriodic()) {
            unit.stopPeriodicMeasurement();
            unit.startPeriodicMeasurement(current_resolution, unit.mtreg());
        } else {
            // SingleShot mode: take a fresh shot with the new resolution.
            Data d{};
            if (unit.measureSingleShot(d, current_resolution, unit.mtreg())) {
                last_singleshot_lux = d.lux();
                last_singleshot_raw = d.raw;
                M5.Log.printf(">singleshot_lux:%.2f\n>singleshot_raw:%u\n", d.lux(), d.raw);
            }
        }
        need_redraw = true;
    }

    if (need_redraw) {
        draw_status();
    }
}
