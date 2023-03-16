##
## Lightdesk, DMX and Headunits
##

idf_component_register(
  SRCS            cmd.cpp
  INCLUDE_DIRS    include
  REQUIRES        io ru_base
  PRIV_REQUIRES   async_msg desk_msg ota
)
