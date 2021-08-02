##
## OTA
##

idf_component_register(
  SRCS ota.cpp
  INCLUDE_DIRS include
  REQUIRES misc esp_http_client app_update esp_https_ota)

set_property(TARGET ${COMPONENT_LIB} PROPERTY CXX_STANDARD 17)
