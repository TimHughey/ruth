##
## OTA
##

idf_component_register(
  SRCS          ota.cpp
  INCLUDE_DIRS  include
  PRIV_REQUIRES 
    app_update
    esp_app_format
    esp_http_client
    esp_https_ota
    esp_timer
    spi_flash
    
  REQUIRES      misc ru_base
)
