##
## Engine I2C
##

idf_component_register(
  SRCS i2c.cpp
  INCLUDE_DIRS include
  REQUIRES ruth_mqtt message misc dev_i2c)

set_property(TARGET ${COMPONENT_LIB} PROPERTY CXX_STANDARD 17)
