##
## Core
##

idf_component_register(
  SRCS
    boot_msg.cpp
    core.cpp
    engines.cpp
    run_msg.cpp
    sntp.cpp
    startup_msg.cpp
  INCLUDE_DIRS
    include
  REQUIRES
    app_update
    arduino_json
    binder
    dev_pwm
    # engine_ds
    # engine_i2c
    # engine_pwm
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
