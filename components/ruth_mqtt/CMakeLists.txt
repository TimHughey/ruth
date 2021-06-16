##
## Ruth MQTT
##

idf_component_register(
  SRCS mqtt.cpp
  INCLUDE_DIRS .
  REQUIRES message misc mqtt)

set_property(TARGET ${COMPONENT_LIB} PROPERTY CXX_STANDARD 17)
target_compile_definitions(${COMPONENT_LIB} PUBLIC  ARDUINOJSON_USE_LONG_LONG)
target_include_directories(${COMPONENT_LIB} PUBLIC ../external ../misc)
