/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*!
  @file unit_Light_data.hpp
  @brief UnitLight (SKU:U021) data types (default values and Data struct)

  Header-only; does not depend on M5UnitComponent, so it can be included from native (non-ESP32) builds.
*/
#ifndef M5_UNIT_LIGHT_UNIT_LIGHT_DATA_HPP
#define M5_UNIT_LIGHT_UNIT_LIGHT_DATA_HPP

#include <cstdint>
#include <limits>

#include <m5_utility/stl/extension.hpp>

namespace m5 {
namespace unit {

/*!
  @namespace light
  @brief For UnitLight (U021)
 */
namespace light {

///@name Default values
///@{
constexpr uint32_t DEFAULT_INTERVAL_MS{50};  //!< Default sampling interval (ms)
constexpr uint16_t DEFAULT_DARK{0};          //!< Default dark reference (ADC value)
constexpr uint16_t DEFAULT_BRIGHT{4095};     //!< Default bright reference (ADC value, 12-bit full scale)
///@}

/*!
  @struct Data
  @brief Measurement data group
 */
struct Data {
    uint16_t analog_raw{};  //!< Raw ADC value from the photoresistor path
    bool digital{};         //!< Comparator threshold output (active level is board-specific; check it on the board)
    uint16_t dark{DEFAULT_DARK};      //!< ADC reading captured when the sensor was covered (dark reference)
    uint16_t bright{DEFAULT_BRIGHT};  //!< ADC reading captured when the sensor was lit (bright reference)

    /*!
      @brief Normalized brightness in 0..100 (%), where 0% = dark reference and 100% = bright reference
      @return Normalized brightness 0..100 (%), clamped; NaN when dark == bright
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
        const float v{(static_cast<float>(analog_raw) - static_cast<float>(dark)) * 100.0f / span};
        return m5::stl::clamp(v, 0.0f, 100.0f);
    }
};

}  // namespace light
}  // namespace unit
}  // namespace m5

#endif
