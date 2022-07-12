#
# asio wrapper
#

idf_component_register(
  SRCS          io.cpp
  INCLUDE_DIRS  include
  REQUIRES      asio
)

set_property(TARGET ${COMPONENT_LIB} PROPERTY CXX_STANDARD 17)
