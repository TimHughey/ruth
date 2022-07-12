idf_component_register(
  SRCS            main.cpp 
  REQUIRES        core nvs_flash)

set_property(TARGET ${COMPONENT_LIB} PROPERTY CXX_STANDARD 17)
