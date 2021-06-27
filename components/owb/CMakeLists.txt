##
## Core
##

idf_component_register(
  SRCS owb_gpio.c owb_rmt.c owb.c
  INCLUDE_DIRS include)

set_property(TARGET ${COMPONENT_LIB} PROPERTY CXX_STANDARD 17)
