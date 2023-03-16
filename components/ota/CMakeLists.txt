##
## OTA
##

idf_component_register(
  SRCS
    ota.cpp

  INCLUDE_DIRS
    include
   
  PRIV_REQUIRES
    async_msg
    desk_msg

  REQUIRES
    app_update
    arduino_json
    binder
    esp_app_format
    esp_http_client
    esp_https_ota
    esp_timer
    spi_flash
    io misc ru_base
    
  EMBED_TXTFILES
    ca.pem
)
