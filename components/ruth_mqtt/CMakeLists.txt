##
## Ruth MQTT
##

idf_component_register(
  SRCS mqtt.cpp
  INCLUDE_DIRS include
  REQUIRES message
  PRIV_REQUIRES mqtt)

set_property(TARGET ${COMPONENT_LIB} PROPERTY CXX_STANDARD 17)
