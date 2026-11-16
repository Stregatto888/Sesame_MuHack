# Adafruit BusIO 1.17.4 (sottomodulo in Adafruit_BusIO/), dipendenza di Adafruit GFX
idf_component_register(SRCS "Adafruit_BusIO/Adafruit_I2CDevice.cpp"
                            "Adafruit_BusIO/Adafruit_BusIO_Register.cpp"
                            "Adafruit_BusIO/Adafruit_SPIDevice.cpp"
                            "Adafruit_BusIO/Adafruit_GenericDevice.cpp"
                       INCLUDE_DIRS "Adafruit_BusIO"
                       REQUIRES arduino-esp32)
