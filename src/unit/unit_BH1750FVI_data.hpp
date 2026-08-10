/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*!
  @file unit_BH1750FVI_data.hpp
  @brief BH1750FVI data types (enums, constants, Data struct)

  Split out from unit_BH1750FVI.hpp so pure formula tests can include it
  without pulling in M5UnitComponent (which is ESP32-only). Native (SDL)
  builds link data_test.cpp against these inline definitions.
*/
#ifndef M5_UNIT_LIGHT_UNIT_BH1750FVI_DATA_HPP
#define M5_UNIT_LIGHT_UNIT_BH1750FVI_DATA_HPP

#include <cstdint>
#include <limits>

namespace m5 {
namespace unit {

/*!
  @namespace bh1750fvi
  @brief For BH1750FVI
 */
namespace bh1750fvi {

/*!
  @enum Mode
  @brief Measurement mode
 */
enum class Mode : uint8_t {
    Continuous,  //!< Continuous measurement. Sensor updates the data register periodically.
    OneTime,     //!< One-time measurement. Sensor enters Power Down after the conversion completes.
};

/*!
  @enum Resolution
  @brief Resolution of the measurement
  @note High2 uses 0.5 lx resolution; raw is interpreted with an extra fractional bit.
 */
enum class Resolution : uint8_t {
    Low,    //!< 4 lx resolution, ~16 ms (max 24 ms) conversion time
    High,   //!< 1 lx resolution, ~120 ms (max 180 ms) conversion time
    High2,  //!< 0.5 lx resolution, ~120 ms (max 180 ms) conversion time
};

//! @brief Minimum value of MTREG (sensitivity register)
constexpr uint8_t MTREG_MIN{31};
//! @brief Maximum value of MTREG
constexpr uint8_t MTREG_MAX{254};
//! @brief Default value of MTREG
constexpr uint8_t MTREG_DEFAULT{69};

//! @brief Minimum sensitivity factor (MTREG_MIN / MTREG_DEFAULT)
constexpr float SENSITIVITY_FACTOR_MIN{0.45f};
//! @brief Maximum sensitivity factor (MTREG_MAX / MTREG_DEFAULT)
constexpr float SENSITIVITY_FACTOR_MAX{3.68f};

/*!
  @struct Data
  @brief Measurement data group
 */
struct Data {
    uint16_t raw{};                           //!< Raw 16-bit ADC value (MSB first)
    uint8_t mtreg{MTREG_DEFAULT};             //!< MTreg value in effect at the time of measurement
    Resolution resolution{Resolution::High};  //!< Resolution in effect at the time of measurement

    //! @brief Illuminance in lx
    //! @details lux = raw / 1.2 * (69 / mtreg) [/ 2 if High2]
    //! @return Illuminance in lx (NaN if mtreg == 0)
    inline float lux() const
    {
        if (mtreg == 0) {
            return std::numeric_limits<float>::quiet_NaN();
        }
        const float base{static_cast<float>(raw) / 1.2f};
        const float scale{static_cast<float>(MTREG_DEFAULT) / static_cast<float>(mtreg)};
        float v{base * scale};
        if (resolution == Resolution::High2) {
            v *= 0.5f;
        }
        return v;
    }
};

}  // namespace bh1750fvi
}  // namespace unit
}  // namespace m5

#endif
