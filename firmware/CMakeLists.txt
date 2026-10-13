# Host build: the Mac simulator and the unit tests (a board build comes later).
cmake_minimum_required(VERSION 3.24)
project(cubey LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)
set(LV_BUILD_CONF_PATH ${CMAKE_CURRENT_SOURCE_DIR}/platforms/mac/lv_conf.h CACHE PATH "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_USE_THORVG OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_USE_SDL ON CACHE BOOL "" FORCE)
set(LV_FETCH_DEPENDENCIES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(lvgl GIT_REPOSITORY https://github.com/lvgl/lvgl.git GIT_TAG v9.6.0 GIT_SHALLOW TRUE)
FetchContent_Declare(doctest GIT_REPOSITORY https://github.com/doctest/doctest.git GIT_TAG v2.5.3 GIT_SHALLOW TRUE)
# cJSON: the same library ESP-IDF ships as its json component. Sources only; core compiles cJSON.c.
FetchContent_Declare(cjson GIT_REPOSITORY https://github.com/DaveGamble/cJSON.git GIT_TAG v1.7.19 GIT_SHALLOW TRUE
    SOURCE_SUBDIR none)
FetchContent_MakeAvailable(lvgl doctest cjson)

add_subdirectory(core)
add_subdirectory(platforms/mac)

enable_testing()
add_subdirectory(tests)
