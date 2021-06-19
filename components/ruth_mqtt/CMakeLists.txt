##
## Ruth MQTT
##

idf_component_register(
  SRCS mqtt.cpp
  INCLUDE_DIRS .
  REQUIRES message misc mqtt)

set_property(TARGET ${COMPONENT_LIB} PROPERTY CXX_STANDARD 17)
