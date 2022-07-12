##
## Device Dallas Semiconductor
##

idf_component_register(
  SRCS bus.cpp crc.cpp ds.cpp ds1820.cpp ds2408.cpp celsius_msg.cpp
  INCLUDE_DIRS include  
  REQUIRES base misc
  PRIV_REQUIRES ruth_mqtt message owb)

set_property(TARGET ${COMPONENT_LIB} PROPERTY CXX_STANDARD 17)
