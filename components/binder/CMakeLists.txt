##
## Binder
##

if(DEFINED ENV{RUTH_BINDER})
  SET(BINDER_MP $ENV{RUTH_BINDER})
else()
  SET(BINDER_MP $ENV{HOME}/devel/shell/local/ruth/embed/binder_0.mp)
endif()

idf_component_register(
  SRCS binder.cpp
  INCLUDE_DIRS .
  REQUIRES arduino_json
  EMBED_TXTFILES ${BINDER_MP})

set_property(TARGET ${COMPONENT_LIB} PROPERTY CXX_STANDARD 17)
