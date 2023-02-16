#
# asio wrapper
#

idf_component_register(
  SRCS          io.cpp
  INCLUDE_DIRS  include
  REQUIRES      asio
)
