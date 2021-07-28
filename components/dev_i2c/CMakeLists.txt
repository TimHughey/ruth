##
## Device I2c
##

idf_component_register(
  SRCS bus.cpp i2c.cpp sht31.cpp ack_msg.cpp relhum_msg.cpp states_msg.cpp
  INCLUDE_DIRS include
  PRIV_INCLUDE_DIRS .
  REQUIRES misc
  PRIV_REQUIRES ruth_mqtt message)

set_property(TARGET ${COMPONENT_LIB} PROPERTY CXX_STANDARD 17)
