##
## Filter
##

idf_component_register(
  SRCS filter.cpp builder.cpp split.cpp in.cpp out.cpp subscribe.cpp
  INCLUDE_DIRS include)

set_property(TARGET ${COMPONENT_LIB} PROPERTY CXX_STANDARD 17)
