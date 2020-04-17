
set(
  RUTH_MISC
    "src/misc/restart"      "src/misc/nvs"
    "src/misc/timestamp_task"   "src/misc/status_led"
    "src/misc/hw_config")

set(
  RUTH_NET
    "src/net/net")

set(
  RUTH_DEVS
    "src/devs/addr"   "src/devs/base"
    "src/devs/ds"     "src/devs/i2c"
    "src/devs/pwm")

set(
  RUTH_CMDS
    "src/cmds/base"     "src/cmds/factory"
    "src/cmds/network"  "src/cmds/ota"
    "src/cmds/pwm"      "src/cmds/queues"
    "src/cmds/switch"   "src/cmds/types")

set(
  RUTH_ENGINES
    "src/engines/i2c"   "src/engines/ds"
    "src/engines/pwm")

set(
  RUTH_READINGS
    "src/readings/celsius"      "src/readings/reading"
    "src/readings/humidity"     "src/readings/soil"
    "src/readings/ramutil"      "src/readings/startup"
    "src/readings/simple_text"  "src/readings/positions"
    "src/readings/remote"       "src/readings/engine"
    "src/readings/pwm")

set(
  RUTH_PROTOCOLS
    "src/protocols/mqtt"  "src/protocols/mqtt_in")

set(
  RUTH_EXTERNAL_LIBS
    "src/libs/mongoose.c")

set(
  RUTH_DRIVERS
    "src/drivers/owb_gpio.c"  "src/drivers/owb_rmt.c"
    "src/drivers/owb.c")

# all the above set() statements build up the sources to register as a
# component for idf to build
idf_component_register(
  SRCS
    ${RUTH_MISC} ${RUTH_NET} ${RUTH_DEVS} ${RUTH_CMDS} ${RUTH_ENGINES}
    ${RUTH_READINGS} ${RUTH_PROTOCOLS} ${RUTH_EXTERNAL_LIBS} ${RUTH_DRIVERS}
  INCLUDE_DIRS
    "include" "include/external"
  REQUIRES
    esp_http_client esp_https_ota nvs_flash app_update esp_adc_cal
  EMBED_FILES
    ca.pem)

target_compile_definitions(${COMPONENT_LIB} PRIVATE
  MG_LOCALS MG_ENABLE_HTTP=0 ARDUINOJSON_ENABLE_STD_STREAM
  ARDUINOJSON_USE_LONG_LONG)
