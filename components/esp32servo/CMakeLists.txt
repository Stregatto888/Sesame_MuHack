# ESP32Servo 3.2.1 (sottomodulo in ESP32Servo/), stessa libreria della build PlatformIO
idf_component_register(SRCS "ESP32Servo/src/ESP32PWM.cpp" "ESP32Servo/src/ESP32Servo.cpp"
                       INCLUDE_DIRS "ESP32Servo/src"
                       REQUIRES arduino-esp32)
