#
# ru_base
#

idf_component_register(
  SRCS          uint8v.cpp
  INCLUDE_DIRS  include
)

target_compile_options(${COMPONENT_LIB} PUBLIC -fconcepts)

set_property(TARGET ${COMPONENT_LIB} PROPERTY CXX_STANDARD 17)
