idf_component_register(
  SRCS status_led.cpp
  INCLUDE_DIRS include)

set_property(TARGET ${COMPONENT_LIB} PROPERTY CXX_STANDARD 17)
