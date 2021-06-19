##
## Core
##

idf_component_register(INCLUDE_DIRS include)

set_property(TARGET ${COMPONENT_LIB} PROPERTY CXX_STANDARD 17)
target_compile_definitions(${COMPONENT_LIB} INTERFACE ARDUINOJSON_USE_LONG_LONG)
