##
## Binder
##

if(DEFINED ENV{RUTH_BINDER})
  SET(BINDER_MP $ENV{RUTH_BINDER})
else()
  SET(BINDER_MP binder_desk.mp)
endif()

idf_component_register(
  SRCS
    binder.cpp
  INCLUDE_DIRS
    include
  REQUIRES
    arduino_json ru_base
  PRIV_REQUIRES
    esp_netif
    esp_hw_support
  EMBED_TXTFILES
    ${BINDER_MP}
)
