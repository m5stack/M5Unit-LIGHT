# M5Unit - LIGHT

## Overview

Library for M5Stack light-related units using [M5UnitUnified](https://github.com/m5stack/M5UnitUnified).
M5UnitUnified is a library for unified handling of various M5 units products.

### SKU:U136
Unit DLight is a digital ambient light detection sensor that uses the BH1750FVI illuminance sensor IC (I2C interface). It has a built-in 16-bit AD converter supporting illuminance value detection ranging from 1 to 65535 lx. It is characterized by its small size and low power consumption, making it suitable for various illuminance detection and light control adjustment scenarios.

### SKU:U134
Hat DLight is a digital ambient light detection sensor compatible with the M5StickC/C Plus series. The hardware uses the BH1750FVI illuminance sensor IC (I2C interface), with a built-in 16-bit AD converter, supporting illuminance value detection from 1 to 65535 lx. It features a small size and low power consumption, making it suitable for various illuminance detection and light control adjustment scenarios.

### SKU:U021
Unit Light is a light intensity detection sensor. It integrates a photoresistor and a 10K adjustable resistor, capable of detecting light intensity and setting a light intensity threshold. The resistance of the photoresistor decreases as the incident light intensity increases, thus detecting the change in voltage, and obtaining light intensity data through AD conversion. To achieve more accurate light intensity measurements, this Unit also adopts the LM393 dual differential comparator, used to compare the differential voltage between the photoresistor and the varistor.

## Related Link
See also examples using conventional methods here.

- [Unit DLight & Datasheet](https://docs.m5stack.com/en/unit/DLight%20Unit)
- [Hat DLight & Datasheet](https://docs.m5stack.com/en/hat/hat_dlight)
- [Unit Light & Datasheet](https://docs.m5stack.com/en/unit/LIGHT)

## Required Libraries

- [M5UnitUnified](https://github.com/m5stack/M5UnitUnified)
- [M5Utility](https://github.com/m5stack/M5Utility)
- [M5HAL](https://github.com/m5stack/M5HAL)

## License

- [M5Unit-LIGHT - MIT](LICENSE)

## Support via [PbHub](https://docs.m5stack.com/en/unit/pbhub_1.1)

|Unit|Support|Note|
|---|---|---|
|UnitLight|OK||

See also [M5Unit-HUB](https://github.com/m5stack/M5Unit-HUB)

## Examples
See also [examples/UnitUnified](examples/UnitUnified)

### For Arduino IDE settings
You must choose a define symbol for the unit you will use.
(Uncomment the corresponding #define in the example, or specify it with compile options.)

- UnitDLight / HatDLight (PlotToSerial)
```cpp
// *************************************************************
// Choose one define symbol to match the unit you are using
// *************************************************************
#if !defined(USING_UNIT_DLIGHT) && !defined(USING_HAT_DLIGHT)
// For UnitDLight (U136)
// #define USING_UNIT_DLIGHT
// For HatDLight (U134)
// #define USING_HAT_DLIGHT
#endif
```

## Doxygen document
[GitHub Pages](https://m5stack.github.io/M5Unit-LIGHT/)

If you want to generate documents on your local machine, execute the following command

```
bash docs/doxy.sh
```

It will output it under docs/html  
If you want to output Git commit hashes to html, do it for the git cloned folder.

### Required
- [Doxygen](https://www.doxygen.nl/)
- [Git](https://git-scm.com/) (Output commit hash to html)
