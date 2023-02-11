##
## Core
##

idf_component_register(
  SRCS core.cpp run_msg.cpp sntp.cpp startup_msg.cpp boot_msg.cpp engines.cpp
  INCLUDE_DIRS include
  REQUIRES
    arduino_json
    binder
    esp_timer
    filter
    message
    ruth_mqtt
    esp_adc
    misc
    network
    app_update
    dev_pwm
    lightdesk
    ota

  )

set_property(TARGET ${COMPONENT_LIB} PROPERTY CXX_STANDARD 17)
