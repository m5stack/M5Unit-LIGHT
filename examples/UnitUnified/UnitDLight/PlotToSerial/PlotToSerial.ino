/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
/*
  Example using M5UnitUnified for UnitDLight / HatDLight (BH1750FVI)
*/
// *************************************************************
// Choose one define symbol to match the unit you are using
// *************************************************************
#if !defined(USING_UNIT_DLIGHT) && !defined(USING_HAT_DLIGHT)
// For UnitDLight (U136)
// #define USING_UNIT_DLIGHT
// For HatDLight (U134)
// #define USING_HAT_DLIGHT
#endif
#include "main/PlotToSerial.cpp"
