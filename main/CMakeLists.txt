# Componente main: usa i sorgenti esistenti in src/ e include/ (gli stessi della build PlatformIO).
file(GLOB_RECURSE SESAME_SRCS "${CMAKE_CURRENT_LIST_DIR}/../src/*.cpp")

idf_component_register(SRCS ${SESAME_SRCS}
                       INCLUDE_DIRS "../include"
                       REQUIRES arduino-esp32 esp32servo adafruit_gfx adafruit_ssd1306)
