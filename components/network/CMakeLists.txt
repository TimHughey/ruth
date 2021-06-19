idf_component_register(
  SRCS network.cpp
  INCLUDE_DIRS .
  REQUIRES misc
  EMBED_TXTFILES ca.pem)

set_property(TARGET ${COMPONENT_LIB} PROPERTY CXX_STANDARD 17)
