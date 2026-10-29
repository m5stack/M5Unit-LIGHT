/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
  Example of using UnitDLight (U136, BH1750FVI) via UnitPaHub

  Board ---> PaHub ---> ch:0 UnitDLight
  Prints the illuminance to the serial output, and draws it when the board has a screen.
*/
#include <algorithm>
#include <cstdio>

#include <M5Unified.h>
#include <M5UnitUnified.h>
#include <M5UnitUnifiedLIGHT.h>
#include <M5UnitUnifiedHUB.h>                 // UnitPaHub
#include <wiring/m5_unit_unified_wiring.hpp>  // Board-aware connection helpers (include last)

namespace {
auto& lcd = M5.Display;

m5::unit::UnitUnified Units;
m5::unit::UnitPaHub hub;
m5::unit::UnitDLight unit;

constexpr uint8_t UNIT_CHANNEL{0};  // PaHub channel the unit is connected to

//! True when the board has a real screen (e.g. Atom Lite / NanoC6 / NanoH2 have none)
bool has_lcd{};

//! One text line, drawn off-screen and pushed at once to avoid flicker
M5Canvas canvas(&lcd);

void setup_canvas()
{
    lcd.fillScreen(TFT_DARKGREEN);
    // 6x8 font scaled so that 12 characters fit the width
    const int32_t text_size = std::max<int32_t>(1, lcd.width() / (12 * 6));
    canvas.setPsram(false);
    canvas.setColorDepth(1);  // Two colors: background and text
    if (!canvas.createSprite(lcd.width(), 8 * text_size)) {
        M5_LOGE("Failed to create canvas sprite");
    }
    canvas.setPaletteColor(0, TFT_DARKGREEN);
    canvas.setPaletteColor(1, TFT_YELLOW);
    canvas.setTextSize(text_size);
    canvas.setTextDatum(middle_center);
    canvas.setTextColor(1);
}

void draw_value(const char* text)
{
    canvas.fillSprite(0);
    canvas.drawString(text, canvas.width() / 2, canvas.height() / 2);
    canvas.pushSprite(0, (lcd.height() - canvas.height()) / 2);
}
}  // namespace

void setup()
{
    M5.begin();
    M5.setTouchButtonHeightByRatio(100);

    // The screen shall be in landscape mode
    if (lcd.height() > lcd.width()) {
        lcd.setRotation(1);
    }

    if (!hub.add(unit, UNIT_CHANNEL)) {  // PaHub ch:0 -> UnitDLight
        M5_LOGE("Failed to add children");
        m5::unit::wiring::failStop();
    }
    // Run the PaHub at the lowest clock of the units behind it: its channel select is sent at that clock, and a
    // read on its own runs at the clock left by the last transfer
    auto hcfg  = hub.component_config();
    hcfg.clock = std::min(hcfg.clock, unit.component_config().clock);
    hub.component_config(hcfg);
    // Board-aware I2C for the PaHub: NessoN1 -> SoftwareI2C on GROVE, NanoC6/NanoH2 -> M5.Ex_I2C, others -> Wire.
    // The unit is reached through the hub (added above), so only the hub is added here.
    if (!m5::unit::wiring::addI2C(Units, hub) || !Units.begin()) {
        M5_LOGE("Failed to begin");
        m5::unit::wiring::failStop();
    }

    M5_LOGI("M5UnitUnified initialized");
    M5_LOGI("%s", Units.debugInfo().c_str());

    // A board without a screen reports a 1x1 dummy display
    has_lcd = (lcd.width() > 8 && lcd.height() > 8);
    if (has_lcd) {
        setup_canvas();
        draw_value("UnitDLight");
    }
}

void loop()
{
    M5.update();
    Units.update();

    if (unit.updated()) {
        // Serial Plotter format
        M5.Log.printf(">lux:%.2f\n>raw:%u\n", unit.lux(), unit.raw());
        if (has_lcd) {
            char buf[16]{};
            snprintf(buf, sizeof(buf), "%.1f lx", unit.lux());
            draw_value(buf);
        }
    }
}

#if !defined(ARDUINO)
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_timer.h>

#if CONFIG_FREERTOS_UNICORE
// Single-core SoCs: run loop() back-to-back, but every ~2 s yield a 5 ms slice so the IDLE task
// runs and feeds the task watchdog (default 5 s).
static inline void feedIdleTaskPeriodically(void)
{
    constexpr uint32_t FEED_INTERVAL_MS{2000};
    constexpr TickType_t FEED_SLEEP_TICKS{pdMS_TO_TICKS(5)};
    static uint32_t s_last_feed_ms{};
    const uint32_t now_ms{static_cast<uint32_t>(esp_timer_get_time() / 1000)};
    if (now_ms - s_last_feed_ms >= FEED_INTERVAL_MS) {
        s_last_feed_ms = now_ms;
        vTaskDelay(FEED_SLEEP_TICKS);
    }
}
#endif

extern "C" void app_main(void)
{
    setup();
    for (;;) {
#if CONFIG_FREERTOS_UNICORE
        feedIdleTaskPeriodically();
#endif
        loop();
    }
}
#endif
