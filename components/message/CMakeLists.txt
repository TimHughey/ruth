##
## Message
##

idf_component_register(
  SRCS out.cpp in.cpp handler.cpp
  INCLUDE_DIRS include
  REQUIRES arduino_json filter)

set_property(TARGET ${COMPONENT_LIB} PROPERTY CXX_STANDARD 17)
