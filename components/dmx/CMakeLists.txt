##
## Dmx
##

idf_component_register(
  SRCS dmx.cpp
  INCLUDE_DIRS . include
REQUIRES asio message misc)

set_property(TARGET ${COMPONENT_LIB} PROPERTY CXX_STANDARD 17)
