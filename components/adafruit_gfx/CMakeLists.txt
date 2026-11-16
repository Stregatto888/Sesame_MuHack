# Adafruit GFX Library 1.13.0 (sottomodulo in Adafruit-GFX-Library/)
idf_component_register(SRCS "Adafruit-GFX-Library/Adafruit_GFX.cpp"
                            "Adafruit-GFX-Library/Adafruit_GrayOLED.cpp"
                            "Adafruit-GFX-Library/Adafruit_SPITFT.cpp"
                            "Adafruit-GFX-Library/glcdfont.c"
                       INCLUDE_DIRS "Adafruit-GFX-Library"
                       REQUIRES arduino-esp32 adafruit_busio)
