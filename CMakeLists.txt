# Sesame MuHack — progetto ESP-IDF 5.5 con Arduino (arduino-esp32 3.3.12) come componente.
# Scheda: Lolin S2 Mini (ESP32-S2FNR2). Build: idf.py nel container sesame_idf (cartella /fw).
cmake_minimum_required(VERSION 3.16)

# Versione fissa: evita "git describe" sulla repo montata nel container (Git solo sull'host).
set(PROJECT_VER "ros-bridge")

# Trasporto e log su USB CDC: componenti esp_usbcdc_transport ed esp_usbcdc_logging
# dell'esempio ufficiale, presi dal sottomodulo micro-ROS (stesso commit fissato).
set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/components/micro_ros_espidf_component/examples/int32_publisher_custom_transport_usbcdc/components")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Compila solo main e i componenti che richiede (come l'esempio ufficiale di Arduino).
idf_build_set_property(MINIMAL_BUILD ON)

project(sesame_muhack)

# esp_tinyusb attiva CONFIG_TINYUSB_CDC_ENABLED: con questa opzione Arduino compilerebbe
# cores/esp32/USBCDC.cpp, che richiede il suo stack USB (assente nella build IDF) e
# ridefinirebbe le callback tud_cdc_*. Arduino non usa la USB (ARDUINO_USB_CDC_ON_BOOT
# non definito), quindi il file si esclude dalla compilazione senza toccare il componente.
idf_component_get_property(arduino_lib espressif__arduino-esp32 COMPONENT_LIB)
idf_component_get_property(arduino_dir espressif__arduino-esp32 COMPONENT_DIR)
set_source_files_properties("${arduino_dir}/cores/esp32/USBCDC.cpp"
                            TARGET_DIRECTORY ${arduino_lib}
                            PROPERTIES HEADER_FILE_ONLY ON)
