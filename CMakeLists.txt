# Sesame MuHack — progetto ESP-IDF 5.5 con Arduino (arduino-esp32 3.3.12) come componente.
# Scheda: Lolin S2 Mini (ESP32-S2FNR2). Build: idf.py nel container sesame_idf (cartella /fw).
# platformio.ini resta per la build Arduino pura finché la migrazione non è verificata.
cmake_minimum_required(VERSION 3.16)

# Versione fissa: evita "git describe" sulla repo montata nel container (Git solo sull'host).
set(PROJECT_VER "ros-bridge")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Compila solo main e i componenti che richiede (come l'esempio ufficiale di Arduino).
idf_build_set_property(MINIMAL_BUILD ON)

project(sesame_muhack)
