# Platform-free core: the runtime, services and UI. A static library on the host, an ESP-IDF component on the board.
if(CMAKE_BUILD_EARLY_EXPANSION)
    # ESP-IDF first reads components only for their requirements. LVGL and cJSON come from the
    # component registry (platforms/esp32/main/idf_component.yml).
    idf_component_register(REQUIRES lvgl__lvgl espressif__cjson)
    return()
endif()

file(GLOB_RECURSE CORE_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_LIST_DIR}/*.cpp)

if(ESP_PLATFORM)
    idf_component_register(SRCS ${CORE_SOURCES} INCLUDE_DIRS . .. REQUIRES lvgl__lvgl espressif__cjson)
else()
    add_library(cubey_core STATIC ${CORE_SOURCES} ${cjson_SOURCE_DIR}/cJSON.c)
    target_include_directories(cubey_core PUBLIC . .. ${cjson_SOURCE_DIR})
    target_link_libraries(cubey_core PUBLIC lvgl)
    target_compile_options(cubey_core PRIVATE -Wall -Wextra $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions -fno-rtti>)
endif()
