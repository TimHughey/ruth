##
## Lightdesk, DMX and Headunits
##

idf_component_register(
  SRCS                ac_power.cpp
  INCLUDE_DIRS        include
  PRIV_INCLUDE_DIRS   .
  REQUIRES            ru_base dev_pwm
)

set_property(TARGET ${COMPONENT_LIB} PROPERTY CXX_STANDARD 17)
