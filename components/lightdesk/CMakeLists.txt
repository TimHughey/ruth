##
## Lightdesk, DMX and Headunits
##

idf_component_register(
  SRCS            dmx.cpp lightdesk.cpp session.cpp
  INCLUDE_DIRS    include
  REQUIRES        io message ru_base
  PRIV_REQUIRES   esp_timer headunit mdns network pthread
)
