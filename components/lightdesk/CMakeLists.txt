##
## Lightdesk, DMX and Headunits
##

idf_component_register(
  SRCS                lightdesk.cpp dmx.cpp advertise.cpp session.cpp
  INCLUDE_DIRS        include
  PRIV_INCLUDE_DIRS   include
  REQUIRES            io ru_base message
  PRIV_REQUIRES       pthread mdns network headunit)

set_property(TARGET ${COMPONENT_LIB} PROPERTY CXX_STANDARD 17)
