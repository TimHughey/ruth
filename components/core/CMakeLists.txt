##
## Core
##

idf_component_register(
  SRCS core.cpp run_msg.cpp sntp.cpp startup_msg.cpp boot_msg.cpp engines.cpp
  INCLUDE_DIRS include
  REQUIRES
    app_update
    arduino_json
    binder
    dev_pwm
    esp_adc
    esp_app_format
    esp_timer
    filter
    lightdesk
    message
    misc
    network
    ota
    ruth_mqtt
  )

set_property(TARGET ${COMPONENT_LIB} PROPERTY CXX_STANDARD 17)
