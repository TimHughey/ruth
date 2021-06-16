idf_component_register(SRCS network.cpp INCLUDE_DIRS . EMBED_TXTFILES ca.pem)

set_property(TARGET ${COMPONENT_LIB} PROPERTY CXX_STANDARD 17)
target_compile_definitions(${COMPONENT_LIB} PRIVATE  ARDUINOJSON_USE_LONG_LONG)
target_include_directories(${COMPONENT_LIB} PRIVATE ../external ../misc)
