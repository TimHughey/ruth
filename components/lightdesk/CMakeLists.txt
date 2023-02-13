##
## Lightdesk, DMX and Headunits
##

idf_component_register(
  SRCS                
    advertise.cpp
    dmx.cpp 
    lightdesk.cpp
    session.cpp
  INCLUDE_DIRS
    include
  PRIV_INCLUDE_DIRS
    include
  REQUIRES
    io
    message
    ru_base
  PRIV_REQUIRES
    esp_timer
    headunit
    mdns
    network
    pthread
  )

set_property(TARGET ${COMPONENT_LIB} PROPERTY CXX_STANDARD 17)
