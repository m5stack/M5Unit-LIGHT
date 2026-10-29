# Map the Kconfig "Target unit" choice (common/Kconfig.variant) to the source-level macro that
# both the Arduino and ESP-IDF builds use. Include this from an example's main/CMakeLists.txt
# *after* idf_component_register() (it needs ${COMPONENT_LIB}). Only the examples that have a
# variant (UnitDLight/PlotToSerial) include this file.
set(M5UNIT_VARIANT "")
if(CONFIG_EXAMPLE_USING_UNIT_DLIGHT)
    set(M5UNIT_VARIANT USING_UNIT_DLIGHT)
elseif(CONFIG_EXAMPLE_USING_HAT_DLIGHT)
    set(M5UNIT_VARIANT USING_HAT_DLIGHT)
endif()
if(M5UNIT_VARIANT)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE ${M5UNIT_VARIANT})
endif()
