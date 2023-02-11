##
## OTA
##

idf_component_register(
  SRCS 
    ota.cpp
  INCLUDE_DIRS
    include
  REQUIRES 
    app_update
    esp_app_format
    esp_http_client
    esp_https_ota
    esp_timer
    misc
    spi_flash
  )

set_property(TARGET ${COMPONENT_LIB} PROPERTY CXX_STANDARD 17)
