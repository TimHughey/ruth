##
## Ruth MQTT
##

idf_component_register(
  SRCS          mqtt.cpp
  INCLUDE_DIRS  include
  REQUIRES      message mqtt ru_base
)
