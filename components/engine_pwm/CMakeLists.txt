##
## Engine PWM
##

idf_component_register(
  SRCS pwm.cpp status_msg.cpp
  INCLUDE_DIRS include
  PRIV_INCLUDE_DIRS .
REQUIRES arduino_json message ruth_mqtt misc dev_pwm)

set_property(TARGET ${COMPONENT_LIB} PROPERTY CXX_STANDARD 17)
