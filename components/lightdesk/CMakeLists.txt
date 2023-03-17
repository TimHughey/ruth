##
## Lightdesk, DMX and Headunits
##

idf_component_register(
  SRCS            dmx.cpp lightdesk.cpp session.cpp
  INCLUDE_DIRS    include
  REQUIRES        io ru_base
  PRIV_REQUIRES   async_msg desk_msg desk_cmd binder esp_timer headunit mdns network
)
