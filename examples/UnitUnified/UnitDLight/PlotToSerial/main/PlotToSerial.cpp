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
#include <M5Unified.h>
#include <M5UnitUnified.h>
#include <M5UnitUnifiedLIGHT.h>
#include <Wire.h>
#include <M5HAL.hpp>  // For NessoN1
#include <cmath>      // std::isnan
#include <limits>     // std::numeric_limits

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

#if defined(USING_HAT_DLIGHT)
struct I2cPins {
    int sda;
    int scl;
};

I2cPins get_hat_i2c_pins(const m5::board_t board)
{
    switch (board) {
        case m5::board_t::board_M5StickC:
        case m5::board_t::board_M5StickCPlus:
        case m5::board_t::board_M5StickCPlus2:
            return {0, 26};
        case m5::board_t::board_M5StickS3:
            return {8, 0};
        case m5::board_t::board_M5StackCoreInk:
            return {25, 26};
        case m5::board_t::board_ArduinoNessoN1:
            return {6, 7};
        default:
            return {-1, -1};
    }
}
#endif

}  // namespace

void setup()
{
    auto m5cfg{M5.config()};
#if defined(USING_HAT_DLIGHT)
    m5cfg.pmic_button  = false;  // Disable BtnPWR on StickC series
    m5cfg.internal_imu = false;
    m5cfg.internal_rtc = false;
#endif

    M5.begin(m5cfg);
    M5.setTouchButtonHeightByRatio(100);

    if (lcd.height() > lcd.width()) {
        lcd.setRotation(1);
    }

    auto board{M5.getBoard()};

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
    const auto pins{get_hat_i2c_pins(board)};
    M5_LOGI("getHatPin: SDA:%d SCL:%d", pins.sda, pins.scl);
    if (pins.sda < 0 || pins.scl < 0) {
        M5_LOGE("Illegal pin number");
        lcd.fillScreen(TFT_RED);
        while (true) {
            m5::utility::delay(10000);
        }
    }

    pinMode(pins.scl, OUTPUT);

    auto& wire{(board == m5::board_t::board_ArduinoNessoN1) ? Wire1 : Wire};
    wire.end();
    wire.begin(pins.sda, pins.scl, 400 * 1000U);
    //    m5::utility::delay(50);  // Allow BH1750 VCC rails to settle after Wire.begin().
    if (!Units.add(unit, wire) || !Units.begin()) {
        M5_LOGE("Failed to begin");
        lcd.fillScreen(TFT_RED);
        while (true) {
            m5::utility::delay(10000);
        }
    }
#else
    // NessoN1: Use SoftwareI2C via M5HAL for GROVE port.
    // NanoC6: Use M5.Ex_I2C (m5::I2C_Class) directly.
    bool unit_ready{};
    if (board == m5::board_t::board_ArduinoNessoN1) {
        auto pin_num_sda{M5.getPin(m5::pin_name_t::port_b_out)};
        auto pin_num_scl{M5.getPin(m5::pin_name_t::port_b_in)};
        M5_LOGI("getPin(M5HAL): SDA:%d SCL:%d", pin_num_sda, pin_num_scl);
        m5::hal::bus::I2CBusConfig i2c_cfg;
        i2c_cfg.pin_sda = m5::hal::gpio::getPin(pin_num_sda);
        i2c_cfg.pin_scl = m5::hal::gpio::getPin(pin_num_scl);
        auto i2c_bus{m5::hal::bus::i2c::getBus(i2c_cfg)};
        M5_LOGI("Bus:%d", i2c_bus.has_value());
        unit_ready = Units.add(unit, i2c_bus ? i2c_bus.value() : nullptr) && Units.begin();
    } else if (board == m5::board_t::board_M5NanoC6) {
        M5_LOGI("Using M5.Ex_I2C");
        unit_ready = Units.add(unit, M5.Ex_I2C) && Units.begin();
    } else {
        auto pin_num_sda{M5.getPin(m5::pin_name_t::port_a_sda)};
        auto pin_num_scl{M5.getPin(m5::pin_name_t::port_a_scl)};
        M5_LOGI("getPin: SDA:%d SCL:%d", pin_num_sda, pin_num_scl);
        Wire.end();
        Wire.begin(pin_num_sda, pin_num_scl, 400 * 1000U);
        //        m5::utility::delay(50);  // Allow BH1750 VCC rails to settle after Wire.begin().
        unit_ready = Units.add(unit, Wire) && Units.begin();
    }
    if (!unit_ready) {
        M5_LOGE("Failed to begin");
        lcd.fillScreen(TFT_RED);
        while (true) {
            m5::utility::delay(10000);
        }
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
