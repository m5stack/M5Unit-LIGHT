/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*!
  @file unit_Light.hpp
  @brief UnitLight (SKU:U021) for M5UnitUnified

  U021 combines a photoresistor, a 10 kΩ adjustable resistor, and an LM393 comparator.
  It exposes one analog (photoresistor voltage) and one digital (comparator threshold) signal.
*/
#ifndef M5_UNIT_LIGHT_UNIT_LIGHT_HPP
#define M5_UNIT_LIGHT_UNIT_LIGHT_HPP

#include <M5UnitComponent.hpp>
#include <m5_utility/stl/extension.hpp>
#include <m5_utility/container/circular_buffer.hpp>
#include "unit_Light_data.hpp"  // Data (with inline normalized())
#include <cstdint>
#include <limits>

namespace m5 {
namespace unit {

/*!
  @class m5::unit::UnitLight
  @brief Analog light-level detector with a comparator threshold output (U021)
 */
class UnitLight : public Component, public PeriodicMeasurementAdapter<UnitLight, light::Data> {
    M5_UNIT_COMPONENT_HPP_BUILDER(UnitLight, 0x00);

public:
    /*!
      @struct config_t
      @brief Settings for begin
     */
    struct config_t {
        //! Start periodic measurement on begin?
        bool start_periodic{true};
        //! Sampling interval in milliseconds
        uint32_t interval_ms{50};
        //! Dark reference used for normalization (default 0)
        uint16_t dark{0};
        //! Bright reference used for normalization (default 4095 for 12-bit ADC)
        uint16_t bright{4095};
    };

    /*! @brief Constructor
        @param addr Placeholder (GPIO unit; address is ignored) */
    explicit UnitLight(const uint8_t addr = DEFAULT_ADDRESS)
        : Component(addr), _data{new m5::container::CircularBuffer<light::Data>(1)}
    {
        auto ccfg        = component_config();
        ccfg.stored_size = 1;
        component_config(ccfg);
    }
    /*! @brief Destructor */
    virtual ~UnitLight()
    {
    }

    //! @brief Begin the unit (configures RX as analog input, TX as digital input)
    //! @return True if successful
    virtual bool begin() override;
    //! @brief Update the unit
    //! @param force If true, force a read regardless of the interval
    virtual void update(const bool force = false) override;

    ///@name Settings for begin
    ///@{
    //! @brief Gets the configuration
    //! @return Current configuration
    inline config_t config() const
    {
        return _cfg;
    }
    //! @brief Set the configuration
    //! @param cfg Configuration to apply
    inline void config(const config_t& cfg)
    {
        _cfg = cfg;
    }
    ///@}

    ///@name Measurement data by periodic
    ///@{
    //! @brief Latest analog raw value
    //! @return Raw ADC value (0 if no data)
    //! @note Raw ADC is chip-generation dependent: ESP32 classic saturates near 4095 (non-linear above ~2.45 V),
    //!       while ESP32-C6/H2/S3/P4 read linearly up to 3.3 V (so dark values may not reach 4095 on those chips).
    //!       Use normalized() with calibrateDark()/calibrateBright() for portable 0-100% readings.
    //!       See: https://developer.espressif.com/blog/2025/08/adc-performance/
    inline uint16_t analog() const
    {
        return !empty() ? latest().analog_raw : 0;
    }
    //! @brief Latest digital threshold output
    //! @return Comparator output (false if no data)
    //! @note Comparator output polarity depends on board revision / trimpot setting; do not assume true == bright.
    inline bool digital() const
    {
        return !empty() ? latest().digital : false;
    }
    //! @brief Latest normalized light level (0..100), recomputed against the current calibration
    //! @return Normalized percentage (NaN if no data or dark == bright)
    //! @note Uses the currently configured `_dark` / `_bright` references rather than the values
    //!       stored at measurement time, so calibrateDark/Bright/setCalibration/resetCalibration
    //!       affect the next normalized() call immediately (no stale buffered output).
    inline float normalized() const
    {
        if (empty()) return std::numeric_limits<float>::quiet_NaN();
        light::Data d = latest();
        d.dark        = _dark;
        d.bright      = _bright;
        return d.normalized();
    }
    ///@}

    ///@name Periodic measurement
    ///@{
    /*! @brief Start periodic measurement in the current settings
        @return True if successful */
    inline bool startPeriodicMeasurement()
    {
        return PeriodicMeasurementAdapter<UnitLight, light::Data>::startPeriodicMeasurement();
    }
    /*!
      @brief Start periodic measurement
      @param interval_ms Sampling interval in milliseconds
      @return True if successful
     */
    inline bool startPeriodicMeasurement(const uint32_t interval_ms)
    {
        return PeriodicMeasurementAdapter<UnitLight, light::Data>::startPeriodicMeasurement(interval_ms);
    }
    /*! @brief Stop periodic measurement
        @return True if successful */
    inline bool stopPeriodicMeasurement()
    {
        return PeriodicMeasurementAdapter<UnitLight, light::Data>::stopPeriodicMeasurement();
    }
    ///@}

    ///@name Calibration
    ///@{
    /*! @brief Record the current analog reading as the dark reference
        @return True if successful
        @note Calibration is stored in RAM only; persistence is the user's responsibility. */
    bool calibrateDark();
    /*! @brief Record the current analog reading as the bright reference
        @return True if successful
        @note Calibration is stored in RAM only; persistence is the user's responsibility. */
    bool calibrateBright();
    /*! @brief Set dark/bright references explicitly
        @param dark Dark reference (ADC value measured under covered sensor)
        @param bright Bright reference (ADC value measured under bright exposure)
        @note Calibration is stored in RAM only; persistence is the user's responsibility. */
    void setCalibration(const uint16_t dark, const uint16_t bright);
    /*! @brief Reset calibration (dark=0, bright=4095)
        @note Calibration is stored in RAM only; persistence is the user's responsibility. */
    void resetCalibration();

    //! @brief Current dark reference
    //! @return Dark reference ADC value
    inline uint16_t dark() const
    {
        return _dark;
    }
    //! @brief Current bright reference
    //! @return Bright reference ADC value
    inline uint16_t bright() const
    {
        return _bright;
    }
    ///@}

protected:
    bool start_periodic_measurement();
    bool start_periodic_measurement(const uint32_t interval_ms);
    bool stop_periodic_measurement();

    bool read_measurement(light::Data& data);

    M5_UNIT_COMPONENT_PERIODIC_MEASUREMENT_ADAPTER_HPP_BUILDER(UnitLight, light::Data);

protected:
    std::unique_ptr<m5::container::CircularBuffer<light::Data>> _data{};
    config_t _cfg{};
    uint16_t _dark{0};
    uint16_t _bright{4095};
};

}  // namespace unit
}  // namespace m5

#endif
