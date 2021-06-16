##
## Binder
##

idf_component_register(SRCS binder.cpp INCLUDE_DIRS . REQUIRES fatfs EMBED_TXTFILES binder_0.mp)

set_property(TARGET ${COMPONENT_LIB} PROPERTY CXX_STANDARD 17)
target_compile_definitions(${COMPONENT_LIB} PRIVATE  ARDUINOJSON_USE_LONG_LONG)
target_include_directories(${COMPONENT_LIB} PUBLIC ../external ../misc)
