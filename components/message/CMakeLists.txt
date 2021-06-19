##
## Message
##

idf_component_register(
  SRCS out.cpp in.cpp handler.cpp
  INCLUDE_DIRS .
REQUIRES filter)

set_property(TARGET ${COMPONENT_LIB} PROPERTY CXX_STANDARD 17)
target_include_directories( ${COMPONENT_LIB} PUBLIC ../external)
target_compile_definitions(${COMPONENT_LIB} PUBLIC ARDUINOJSON_USE_LONG_LONG)
