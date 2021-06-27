##
## Device PWM
##

idf_component_register(
  SRCS pwm.cpp hardware.cpp cmd.cpp cmd_random.cpp
  INCLUDE_DIRS include
  REQUIRES arduino_json misc)

set_property(TARGET ${COMPONENT_LIB} PROPERTY CXX_STANDARD 17)
