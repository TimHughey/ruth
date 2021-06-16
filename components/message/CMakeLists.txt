##
## Message
##

idf_component_register(
  SRCS boot.cpp filter.cpp out.cpp in.cpp handler.cpp
  INCLUDE_DIRS .
  REQUIRES binder network app_update)

set_property(TARGET ${COMPONENT_LIB} PROPERTY CXX_STANDARD 17)
target_compile_definitions(${COMPONENT_LIB} PUBLIC ARDUINOJSON_USE_LONG_LONG)
