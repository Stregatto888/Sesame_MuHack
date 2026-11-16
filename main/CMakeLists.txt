# Componente main: usa i sorgenti in src/ e include/ (src/ros/ è solo per la build IDF).
file(GLOB_RECURSE SESAME_SRCS "${CMAKE_CURRENT_LIST_DIR}/../src/*.cpp")

idf_component_register(SRCS ${SESAME_SRCS}
                       INCLUDE_DIRS "../include"
                       REQUIRES arduino-esp32 esp32servo adafruit_gfx adafruit_ssd1306
                                micro_ros_espidf_component esp_usbcdc_transport esp_usbcdc_logging
                                esp_tinyusb)
