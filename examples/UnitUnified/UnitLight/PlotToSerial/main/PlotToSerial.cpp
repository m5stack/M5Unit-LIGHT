/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
  Example using M5UnitUnified for UnitLight (U021)

  BtnA click: capture dark reference
  BtnA hold:  capture bright reference
*/
#include <M5Unified.h>
#include <M5UnitUnified.h>
#include <M5UnitUnifiedLIGHT.h>
#include <wiring/m5_unit_unified_wiring.hpp>  // Board-aware connection helpers (include last)

namespace {
auto& lcd = M5.Display;
M5Canvas canvas(&lcd);  // off-screen buffer to prevent flicker

m5::unit::UnitUnified Units;
m5::unit::UnitLight unit;

constexpr uint32_t DRAW_INTERVAL_MS{200};
uint32_t last_draw_ms{0};

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

    if (wide) {
        canvas.printf("Unit Light\n\n");

        // Main readings are emphasized (text size 2).
        canvas.setTextSize(2);
        canvas.printf("ana: %u\n", unit.analog());
        canvas.printf("dig: %u\n", unit.digital() ? 1u : 0u);
        canvas.printf("nrm: %.1f%%\n", unit.normalized());

        // Calibration references at the default size.
        canvas.setTextSize(1);
        canvas.printf("\ndark:   %u\nbright: %u\n", unit.dark(), unit.bright());
        canvas.printf("\nBtnA click: dark\nBtnA hold:  bright\n");
    } else {
        // Compact layout for small screens (AtomS3 128x128 etc.)
        canvas.printf("UnitLight\n");

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

    // U021 uses both analog input (rx) and digital comparator output (tx); PortB preferred, PortA fallback.
    if (!m5::unit::wiring::addGPIO(Units, unit) || !Units.begin()) {
        M5_LOGE("Failed to begin");
        m5::unit::wiring::failStop();
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
        // Throttle LCD redraws to avoid flicker from periodic measurements.
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
