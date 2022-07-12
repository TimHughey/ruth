##
## Device I2c
##

idf_component_register(
  SRCS bus.cpp i2c.cpp sht31.cpp mcp23008.cpp relhum_msg.cpp
  INCLUDE_DIRS include
  REQUIRES dev_i2c
  PRIV_REQUIRES ruth_mqtt message)

set_property(TARGET ${COMPONENT_LIB} PROPERTY CXX_STANDARD 17)
