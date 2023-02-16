##
## Message
##

idf_component_register(
  SRCS
    ack_msg.cpp
    handler.cpp
    in.cpp
    out.cpp
    states_msg.cpp
  INCLUDE_DIRS
    include
  REQUIRES 
    arduino_json
    esp_timer
    filter
)
