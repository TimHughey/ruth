##
## Device PWM
##

idf_component_register(
  SRCS pwm.cpp hardware.cpp
  INCLUDE_DIRS include
  REQUIRES
    arduino_json
    driver
    misc
)
