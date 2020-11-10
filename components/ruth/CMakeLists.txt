
set(
  RUTH_CORE
    "src/core/binder" "src/core/core"  "src/core/ota"
  )

set(
  RUTH_MISC
    "src/misc/datetime" "src/misc/restart" "src/misc/status_led")

set(
  RUTH_NET
    "src/net/net" "src/net/profile/profile")

set(
  RUTH_DEVS
    "src/devs/base/addr"
    "src/devs/base/base"
    "src/devs/ds/ds"
    "src/devs/i2c/base"
    "src/devs/i2c/mcp23008"
    "src/devs/i2c/mplex"
    "src/devs/i2c/sht31")

set(
  RUTH_PWM_DEVS
    "src/devs/pwm/pwm"
    "src/devs/pwm/cmds/cmd"
    "src/devs/pwm/cmds/basic"
    "src/devs/pwm/cmds/random")

set(
  RUTH_ENGINES
    "src/engines/i2c"   "src/engines/ds"
    "src/engines/pwm"   "src/engines/task")

set(
  RUTH_READINGS
    "src/readings/sensor"  "src/readings/reading"
    "src/readings/startup" "src/readings/pwm"
    "src/readings/text"    "src/readings/positions"
    "src/readings/remote")

set(
  RUTH_PROTOCOLS
    "src/protocols/mqtt"        "src/protocols/payload")

set(
  RUTH_DRIVERS
    "src/drivers/owb_gpio.c"  "src/drivers/owb_rmt.c"
    "src/drivers/owb.c")

# all the above set() statements build up the sources to register as a
# component for idf to build
idf_component_register(
  SRCS
    ${RUTH_CORE} ${RUTH_MISC} ${RUTH_NET} ${RUTH_DEVS} ${RUTH_PWM_DEVS}
    ${RUTH_ENGINES} ${RUTH_READINGS} ${RUTH_PROTOCOLS} ${RUTH_DRIVERS}
  INCLUDE_DIRS
    "include" "include/external"
  REQUIRES
    spiffs esp_http_client esp_https_ota mqtt nvs_flash app_update esp_adc_cal
  EMBED_FILES
    ca.pem)

spiffs_create_partition_image(ruthfs ../../extra/ruthfs/image FLASH_IN_PROJECT)

target_compile_definitions(${COMPONENT_LIB} PRIVATE
  ARDUINOJSON_ENABLE_STD_STREAM ARDUINOJSON_USE_LONG_LONG)
