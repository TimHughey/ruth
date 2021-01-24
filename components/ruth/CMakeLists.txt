set(RUTH_CORE "src/core/binder" "src/core/core" "src/core/ota")
set(RUTH_CLI "src/cli/cli" "src/cli/binder" "src/cli/lightdesk" "src/cli/ota" "src/cli/shell" "src/cli/random" "src/cli/i2s")
set(RUTH_MISC "src/misc/random" "src/misc/status_led")
set(RUTH_NET "src/net/net" "src/net/profile/profile")

set(RUTH_DEVS_BASE "src/devs/base/addr" "src/devs/base/base")
set(RUTH_DEVS_DS "src/devs/ds/base")
set(RUTH_DEVS_I2C "src/devs/i2c/base" "src/devs/i2c/mcp23008" "src/devs/i2c/mplex" "src/devs/i2c/sht31")
set(RUTH_DEVS_PWM "src/devs/pwm/pwm" "src/devs/pwm/cmds/cmd" "src/devs/pwm/cmds/basic" "src/devs/pwm/cmds/random")
set(RUTH_DEVS_DMX "src/devs/dmx/headunit" "src/devs/dmx/pinspot/base")

set(RUTH_ENGINES "src/engines/i2c" "src/engines/ds" "src/engines/pwm")
set(RUTH_LIGHTDESK "src/lightdesk/control" "src/lightdesk/lightdesk" "src/lightdesk/fx" "src/lightdesk/types")

set(RUTH_READINGS "src/readings/sensor" "src/readings/reading" "src/readings/startup" "src/readings/pwm" "src/readings/text" "src/readings/positions" "src/readings/remote")
set(RUTH_PROTOCOLS "src/protocols/dmx" "src/protocols/mqtt" "src/protocols/payload" "src/protocols/i2s")

set(RUTH_DRIVERS "src/drivers/owb_gpio.c" "src/drivers/owb_rmt.c" "src/drivers/owb.c")

# all the above set() statements build up the sources to register as a
# component for idf to build
idf_component_register(
  SRCS
    ${RUTH_CORE} ${RUTH_CLI} ${RUTH_MISC} ${RUTH_NET}
    ${RUTH_DEVS_BASE} ${RUTH_DEVS_DS} ${RUTH_DEVS_I2C} ${RUTH_DEVS_PWM} ${RUTH_DEVS_DMX}
    ${RUTH_ENGINES} ${RUTH_READINGS} ${RUTH_PROTOCOLS} ${RUTH_DRIVERS} ${RUTH_LIGHTDESK}
  INCLUDE_DIRS
    "include" "include/external"
  REQUIRES
    fatfs esp_http_client esp_https_ota mqtt nvs_flash app_update esp_adc_cal console
  EMBED_FILES
    embed/ca.pem embed/binder_0.mp)

# spiffs_create_partition_image(ruthfs ../../extra/ruthfs/image FLASH_IN_PROJECT)
target_compile_definitions(${COMPONENT_LIB} PRIVATE ARDUINOJSON_USE_LONG_LONG)
