/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
  Example of using UnitLight (U021, LM393 + photoresistor) via UnitPbHub

  Core ---> PbHub ---> ch:3 UnitLight

  BtnA click: capture dark reference
  BtnA hold:  capture bright reference
*/
#include <M5Unified.h>
#include <M5UnitUnified.h>
#include <M5HAL.hpp>
#include <M5UnitUnifiedLIGHT.h>
#include <M5UnitUnifiedHUB.h>  // UnitPbHub

namespace {
auto& lcd = M5.Display;
M5Canvas canvas(&lcd);  // off-screen buffer to prevent flicker

m5::unit::UnitUnified Units;
m5::unit::UnitPbHub hub;
m5::unit::UnitLight unit;

constexpr uint32_t DRAW_INTERVAL_MS{200};
uint32_t last_draw_ms{0};

void draw_status()
{
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

    if (wide) {
        canvas.printf("Light via PbHub\n\n");

        canvas.setTextSize(2);
        canvas.printf("ana: %u\n", unit.analog());
        canvas.printf("dig: %u\n", unit.digital() ? 1u : 0u);
        canvas.printf("nrm: %.1f%%\n", unit.normalized());

        canvas.setTextSize(1);
        canvas.printf("\ndark:   %u\nbright: %u\n", unit.dark(), unit.bright());
        canvas.printf("\nBtnA click: dark\nBtnA hold:  bright\n");
    } else {
        canvas.printf("Light/Hub\n");

        canvas.setTextSize(2);
        canvas.printf("ana:%u\n", unit.analog());
        canvas.printf("dig:%u\n", unit.digital() ? 1u : 0u);
        canvas.printf("nrm:%.0f%%\n", unit.normalized());

        canvas.setTextSize(1);
        canvas.printf("\ndrk:%u bri:%u\n", unit.dark(), unit.bright());
        canvas.printf("A:drk  A^:bri\n");
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

    if (!hub.add(unit, 3)) {  // PbHub ch:3 -> UnitLight
        M5_LOGE("Failed to add children");
        lcd.fillScreen(TFT_RED);
        while (true) {
            m5::utility::delay(10000);
        }
    }

    auto board = M5.getBoard();

    // NessoN1: Arduino Wire (I2C_NUM_0) cannot be used for GROVE port.
    //   Wire is used by M5Unified In_I2C for internal devices.
    //   Reconfiguring Wire to GROVE pins breaks In_I2C.
    //   Solution: Use SoftwareI2C via M5HAL for the GROVE port.
    // NanoC6: Wire.begin() on GROVE pins conflicts with m5::I2C_Class
    //   registered by Ex_I2C.setPort() on the same I2C_NUM_0.
    //   Solution: Use M5.Ex_I2C directly instead of Arduino Wire.
    bool unit_ready{};
    if (board == m5::board_t::board_ArduinoNessoN1) {
        // NessoN1: GROVE is port_b (port_a is reserved for internal use)
        auto pin_num_sda = M5.getPin(m5::pin_name_t::port_b_out);
        auto pin_num_scl = M5.getPin(m5::pin_name_t::port_b_in);
        M5_LOGI("getPin(M5HAL): SDA:%u SCL:%u", pin_num_sda, pin_num_scl);
        m5::hal::bus::I2CBusConfig i2c_cfg;
        i2c_cfg.pin_sda = m5::hal::gpio::getPin(pin_num_sda);
        i2c_cfg.pin_scl = m5::hal::gpio::getPin(pin_num_scl);
        auto i2c_bus    = m5::hal::bus::i2c::getBus(i2c_cfg);
        M5_LOGI("Bus:%d", i2c_bus.has_value());
        unit_ready = Units.add(hub, i2c_bus ? i2c_bus.value() : nullptr) && Units.begin();
    } else if (board == m5::board_t::board_M5NanoC6) {
        M5_LOGI("Using M5.Ex_I2C");
        unit_ready = Units.add(hub, M5.Ex_I2C) && Units.begin();
    } else {
        auto pin_num_sda = M5.getPin(m5::pin_name_t::port_a_sda);
        auto pin_num_scl = M5.getPin(m5::pin_name_t::port_a_scl);
        M5_LOGI("getPin: SDA:%u SCL:%u", pin_num_sda, pin_num_scl);
        Wire.end();
        Wire.begin(pin_num_sda, pin_num_scl, 400000U);
        unit_ready = Units.add(hub, Wire) && Units.begin();
    }

    if (!unit_ready) {
        M5_LOGE("Failed to begin");
        lcd.fillScreen(TFT_RED);
        while (true) {
            m5::utility::delay(10000);
        }
    }

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

    M5_LOGI("M5UnitUnified initialized");
    M5_LOGI("%s", Units.debugInfo().c_str());
    lcd.fillScreen(TFT_BLACK);
    draw_status();
}

void loop()
{
    M5.update();
    Units.update();

    bool need_redraw{false};

    if (unit.updated()) {
        M5.Log.printf(">analog:%u\n>digital:%u\n>normalized:%.2f\n", unit.analog(), unit.digital() ? 1u : 0u,
                      unit.normalized());
        const uint32_t now{m5::utility::millis()};
        if (now - last_draw_ms >= DRAW_INTERVAL_MS) {
            last_draw_ms = now;
            need_redraw  = true;
        }
    }

    if (M5.BtnA.wasHold()) {
        if (unit.calibrateBright()) {
            M5_LOGI("bright = %u", unit.bright());
            need_redraw = true;
        }
    } else if (M5.BtnA.wasClicked()) {
        if (unit.calibrateDark()) {
            M5_LOGI("dark = %u", unit.dark());
            need_redraw = true;
        }
    }

    if (need_redraw) {
        draw_status();
    }
}
