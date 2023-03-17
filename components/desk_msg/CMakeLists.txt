##
## Lightdesk, DMX and Headunits
##

idf_component_register(
  SRCS            
    out_info.cpp

  INCLUDE_DIRS
    include

  PRIV_REQUIRES
    esp_app_format

  REQUIRES        
    io
    ru_base
)
