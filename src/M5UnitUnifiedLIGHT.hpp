/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*!
  @file M5UnitUnifiedLIGHT.hpp
  @brief Aggregates light-related M5 Unit drivers and provides convenience aliases.

  Supported products:
  - UnitDLight (SKU:U136) and HatDLight (SKU:U134) — BH1750FVI ambient light sensor (I2C)
  - UnitLight (SKU:U021) — LM393 + photoresistor analog/digital light detector (GPIO)
*/
#ifndef M5_UNIT_UNIFIED_LIGHT_HPP
#define M5_UNIT_UNIFIED_LIGHT_HPP

#include "unit/unit_BH1750FVI.hpp"
#include "unit/unit_Light.hpp"

/*!
  @namespace m5
  @brief Top level namespace of M5Stack
*/
namespace m5 {

/*!
  @namespace unit
  @brief Unit-related namespace
*/
namespace unit {

using UnitDLight = UnitBH1750FVI;  //!< Unit DLight (SKU:U136) — BH1750FVI on GROVE port
using HatDLight  = UnitBH1750FVI;  //!< Hat DLight  (SKU:U134) — BH1750FVI on StickC hat

}  // namespace unit
}  // namespace m5

#endif
