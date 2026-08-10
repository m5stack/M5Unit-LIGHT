/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*!
  @file unit_Light_data.hpp
  @brief UnitLight (SKU:U021) data types (Data struct)

  Split out from unit_Light.hpp so pure formula tests can include it without
  pulling in M5UnitComponent (which is ESP32-only). Native (SDL) builds link
  data_test.cpp against these inline definitions.
*/
#ifndef M5_UNIT_LIGHT_UNIT_LIGHT_DATA_HPP
#define M5_UNIT_LIGHT_UNIT_LIGHT_DATA_HPP

#include <cstdint>
#include <limits>

namespace m5 {
namespace unit {

/*!
  @namespace light
  @brief For UnitLight (U021)
 */
namespace light {

/*!
  @struct Data
  @brief Measurement data group
 */
struct Data {
    uint16_t analog_raw{};  //!< Raw ADC value from the photoresistor path
    bool digital{};         //!< Comparator threshold output (active level is board-specific; verify with calibration)
    uint16_t dark{0};       //!< ADC reading captured when the sensor was covered (dark reference)
    uint16_t bright{4095};  //!< ADC reading captured when the sensor was lit (bright reference)

    /*!
      @brief Normalized brightness in 0..100 (%), where 0% = dark reference and 100% = bright reference
      @return NaN only when dark == bright (cannot form a range)
      @note dark and bright are stored as the raw ADC values captured during calibration; their
            magnitudes do not imply polarity. On U021 the photoresistor circuit polarity is
            board-specific (some revisions yield higher ADC in the dark, others in the light)
            — normalized() handles either polarity and always maps dark to 0% and bright to 100%.
     */
    inline float normalized() const
    {
        if (dark == bright) {
            return std::numeric_limits<float>::quiet_NaN();
        }
        const float span{static_cast<float>(bright) - static_cast<float>(dark)};
        float v{(static_cast<float>(analog_raw) - static_cast<float>(dark)) * 100.0f / span};
        if (v < 0.0f) v = 0.0f;
        if (v > 100.0f) v = 100.0f;
        return v;
    }
};

}  // namespace light
}  // namespace unit
}  // namespace m5

#endif
