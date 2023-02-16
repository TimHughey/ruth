#
# ru_base
#

idf_component_register(
  SRCS          uint8v.cpp
  INCLUDE_DIRS  include
  REQUIRES      misc
)

# target_compile_options(${COMPONENT_LIB} PUBLIC -fconcepts)

