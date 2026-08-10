/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*!
  @file unit_BH1750FVI.hpp
  @brief BH1750FVI Unit for M5UnitUnified

  Shared driver for UnitDLight (SKU:U136) and HatDLight (SKU:U134).
  BH1750FVI is a Rohm digital 16-bit ambient light sensor (I2C).
*/
#ifndef M5_UNIT_LIGHT_UNIT_BH1750FVI_HPP
#define M5_UNIT_LIGHT_UNIT_BH1750FVI_HPP

#include <M5UnitComponent.hpp>
#include <m5_utility/stl/extension.hpp>
#include <m5_utility/container/circular_buffer.hpp>
#include "unit_BH1750FVI_data.hpp"  // Mode, Resolution, MTREG_* constants, Data (with inline lux())
#include <cstdint>
#include <limits>

namespace m5 {
namespace unit {

namespace bh1750fvi {

///@cond
namespace command {
// Power / reset opcodes
constexpr uint8_t POWER_DOWN{0x00};
constexpr uint8_t POWER_ON{0x01};
constexpr uint8_t RESET{0x07};

// Continuous-mode measurement opcodes
constexpr uint8_t CONTINUOUS_H_RES_MODE{0x10};
constexpr uint8_t CONTINUOUS_H_RES_MODE2{0x11};
constexpr uint8_t CONTINUOUS_L_RES_MODE{0x13};

// One-time measurement opcodes
constexpr uint8_t ONE_TIME_H_RES_MODE{0x20};
constexpr uint8_t ONE_TIME_H_RES_MODE2{0x21};
constexpr uint8_t ONE_TIME_L_RES_MODE{0x23};  // NOTE: same hex as slave address, not related

// MTreg opcodes (bitmask bases; combine with mtreg bits)
constexpr uint8_t CHANGE_MEASUREMENT_TIME_HIGH{0x40};  //!< OR with ((mtreg >> 5) & 0x07)
constexpr uint8_t CHANGE_MEASUREMENT_TIME_LOW{0x60};   //!< OR with (mtreg & 0x1F)
}  // namespace command
///@endcond

}  // namespace bh1750fvi

/*!
  @class m5::unit::UnitBH1750FVI
  @brief Digital ambient light sensor
  @details Shared between UnitDLight (SKU:U136) and HatDLight (SKU:U134).
 */
class UnitBH1750FVI : public Component, public PeriodicMeasurementAdapter<UnitBH1750FVI, bh1750fvi::Data> {
    M5_UNIT_COMPONENT_HPP_BUILDER(UnitBH1750FVI, 0x23);

public:
    //! @brief Alternate I2C address when ADDR pin is tied high
    static constexpr uint8_t ALT_ADDRESS{0x5C};

    /*!
      @struct config_t
      @brief Settings for begin
     */
    struct config_t {
        //! Start periodic measurement on begin? (true: Continuous mode, false: standby for one-time/manual control)
        bool start_periodic{true};
        //! Resolution if start on begin
        bh1750fvi::Resolution resolution{bh1750fvi::Resolution::High};
        //! MTreg if start on begin (31..254, default 69)
        uint8_t mtreg{bh1750fvi::MTREG_DEFAULT};
    };

    /*! @brief Constructor
        @param addr I2C address */
    explicit UnitBH1750FVI(const uint8_t addr = DEFAULT_ADDRESS)
        : Component(addr), _data{new m5::container::CircularBuffer<bh1750fvi::Data>(1)}
    {
        auto ccfg        = component_config();
        ccfg.clock       = 400 * 1000U;  // BH1750FVI I2C max clock
        ccfg.stored_size = 8;
        component_config(ccfg);
    }
    /*! @brief Destructor */
    virtual ~UnitBH1750FVI()
    {
    }

    //! @brief Begin the unit
    //! @return True if successful
    virtual bool begin() override;
    //! @brief Update the unit
    //! @param force If true, force a read regardless of the interval
    virtual void update(const bool force = false) override;

    ///@name Settings for begin
    ///@{
    //! @brief Gets the configuration
    //! @return Current configuration
    inline config_t config()
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
    //! @brief Latest illuminance (lx)
    //! @return Illuminance in lx (NaN if no data)
    inline float lux() const
    {
        return !empty() ? latest().lux() : std::numeric_limits<float>::quiet_NaN();
    }
    //! @brief Latest raw value
    //! @return Raw 16-bit ADC value (0 if no data)
    inline uint16_t raw() const
    {
        return !empty() ? latest().raw : 0;
    }
    //! @brief Resolution currently in effect
    //! @return Current resolution
    inline bh1750fvi::Resolution resolution() const
    {
        return _resolution;
    }
    //! @brief MTreg currently in effect
    //! @return Current MTreg
    inline uint8_t mtreg() const
    {
        return _mtreg;
    }
    ///@}

    ///@name Periodic measurement
    ///@{
    /*!
      @brief Start periodic measurement in the current settings
      @return True if successful
    */
    inline bool startPeriodicMeasurement()
    {
        return PeriodicMeasurementAdapter<UnitBH1750FVI, bh1750fvi::Data>::startPeriodicMeasurement();
    }
    /*!
      @brief Start periodic measurement
      @param resolution Resolution
      @param mtreg MTreg value (31..254)
      @return True if successful
    */
    inline bool startPeriodicMeasurement(const bh1750fvi::Resolution resolution, const uint8_t mtreg)
    {
        return PeriodicMeasurementAdapter<UnitBH1750FVI, bh1750fvi::Data>::startPeriodicMeasurement(resolution, mtreg);
    }
    /*!
      @brief Stop periodic measurement
      @return True if successful
    */
    inline bool stopPeriodicMeasurement()
    {
        return PeriodicMeasurementAdapter<UnitBH1750FVI, bh1750fvi::Data>::stopPeriodicMeasurement();
    }
    ///@}

    ///@name Single shot measurement
    ///@{
    /*!
      @brief Measure single-shot
      @param[out] data Measured data
      @param resolution Resolution to use (default: High)
      @param mtreg MTreg value (default: 69)
      @return True if successful
      @warning Blocks until the measurement completes (up to 180 ms * mtreg/69)
     */
    bool measureSingleShot(bh1750fvi::Data& data, const bh1750fvi::Resolution resolution = bh1750fvi::Resolution::High,
                           const uint8_t mtreg = bh1750fvi::MTREG_DEFAULT);
    ///@}

    ///@name MTreg (sensitivity) control
    ///@{
    /*!
      @brief Write MTreg as raw value (31..254)
      @param mtreg MTreg
      @return True if successful
      @note During periodic measurement, the current mode opcode is re-sent after writing MTreg.
     */
    bool writeMTreg(const uint8_t mtreg);

    /*!
      @brief Write MTreg as sensitivity factor (0.45..3.68)
      @param factor Sensitivity factor (default 1.0 corresponds to mtreg=69)
      @return True if successful
     */
    bool writeSensitivityFactor(const float factor);

    //! @brief Gets the current MTreg cache
    //! @return Current MTreg
    inline uint8_t readMTreg() const
    {
        return _mtreg;
    }
    ///@}

    ///@name Power management
    ///@{
    /*! @brief Write POWER_ON opcode
        @return True if successful */
    bool powerOn();
    /*! @brief Write POWER_DOWN opcode
        @return True if successful */
    bool powerDown();
    /*! @brief Write RESET opcode (clears data register; requires POWER_ON state)
        @return True if successful */
    bool softReset();
    ///@}

protected:
    bool start_periodic_measurement();
    bool start_periodic_measurement(const bh1750fvi::Resolution resolution, const uint8_t mtreg);
    bool stop_periodic_measurement();

    bool read_measurement(bh1750fvi::Data& data);
    bool write_opcode(const uint8_t opcode);
    bool write_mtreg_opcodes(const uint8_t mtreg);
    uint8_t opcode_for(const bh1750fvi::Mode mode, const bh1750fvi::Resolution resolution) const;

    void apply_interval(const bh1750fvi::Resolution resolution, const uint8_t mtreg);

    M5_UNIT_COMPONENT_PERIODIC_MEASUREMENT_ADAPTER_HPP_BUILDER(UnitBH1750FVI, bh1750fvi::Data);

protected:
    std::unique_ptr<m5::container::CircularBuffer<bh1750fvi::Data>> _data{};
    config_t _cfg{};
    bh1750fvi::Resolution _resolution{bh1750fvi::Resolution::High};
    uint8_t _mtreg{bh1750fvi::MTREG_DEFAULT};
};

}  // namespace unit
}  // namespace m5

#endif
