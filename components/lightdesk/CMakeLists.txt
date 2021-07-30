##
## Lightdesk
##

idf_component_register(
  SRCS lightdesk.cpp
  INCLUDE_DIRS include
REQUIRES dmx message misc)

set_property(TARGET ${COMPONENT_LIB} PROPERTY CXX_STANDARD 17)
