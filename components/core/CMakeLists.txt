##
## Core
##

idf_component_register(
  SRCS core.cpp   
    boot_msg.cpp
    engines.cpp
    run_msg.cpp
    sntp.cpp
    startup_msg.cpp
  INCLUDE_DIRS    include
  PRIV_REQUIRES   
    app_update
    arduino_json
    binder
    dev_pwm
    esp_adc
    esp_app_format
    esp_timer
    filter
    lightdesk
    misc
    network
    ruth_mqtt
  REQUIRES
    message
    ota
)
