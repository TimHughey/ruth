##
## Engine Dallas Semiconductor
##

idf_component_register(
  SRCS ds.cpp
  INCLUDE_DIRS include
  PRIV_INCLUDE_DIRS .
  REQUIRES ruth_mqtt message misc dev_ds)

set_property(TARGET ${COMPONENT_LIB} PROPERTY CXX_STANDARD 17)
