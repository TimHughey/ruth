idf_component_register(
  SRCS status_led.cpp
  INCLUDE_DIRS include
  REQUIRES dev_pwm misc
)

set_property(TARGET ${COMPONENT_LIB} PROPERTY CXX_STANDARD 17)
