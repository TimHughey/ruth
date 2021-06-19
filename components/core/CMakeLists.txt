##
## Core
##

idf_component_register(
  SRCS core.cpp run_msg.cpp sntp.cpp startup_msg.cpp boot_msg.cpp
  INCLUDE_DIRS .
  REQUIRES arduino_json binder filter message ruth_mqtt esp_adc_cal misc network app_update)

set_property(TARGET ${COMPONENT_LIB} PROPERTY CXX_STANDARD 17)
