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
    arduino_json
  EMBED_TXTFILES
    ${BINDER_MP}
)
