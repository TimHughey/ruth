##
## Lightdesk, DMX and Headunits
##

idf_component_register(
  SRCS                lightdesk.cpp dmx.cpp
  INCLUDE_DIRS        include
  REQUIRES            io base message misc
  PRIV_REQUIRES       pthread headunit mdns)

set_property(TARGET ${COMPONENT_LIB} PROPERTY CXX_STANDARD 17)
