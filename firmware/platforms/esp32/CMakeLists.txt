# Cubey on the board: the Waveshare ESP32-S3-Touch-AMOLED-2.16 (480 x 480).
#   . ~/esp/esp-idf-v6.1/export.sh && idf.py -C firmware/platforms/esp32 -B build/esp32 build
cmake_minimum_required(VERSION 3.22)
set(SDKCONFIG_DEFAULTS sdkconfig.defaults)
# This board's Wi-Fi, server and token, from the git-ignored sdkconfig.local.
if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/sdkconfig.local)
    list(APPEND SDKCONFIG_DEFAULTS sdkconfig.local)
endif()
set(EXTRA_COMPONENT_DIRS ${CMAKE_CURRENT_LIST_DIR}/../../core)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(cubey)
