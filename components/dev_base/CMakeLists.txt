##
## Device Base
##

idf_component_register(
  SRCS addr.cpp base.cpp
  INCLUDE_DIRS include
REQUIRES misc)

set_property(TARGET ${COMPONENT_LIB} PROPERTY CXX_STANDARD 17)
