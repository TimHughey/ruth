##
## Lightdesk, DMX and Headunits
##

idf_component_register(
  SRCS            cmd.cpp ota.cpp
  INCLUDE_DIRS    include
  REQUIRES
    io
    ru_base
    misc
    binder
    desk_msg
    esp_app_format
    esp_http_client
    esp_https_ota
    app_update
    spi_flash

  PRIV_REQUIRES
    async_msg
    arduino_json
    esp_timer

  EMBED_TXTFILES
    ca.pem
)
