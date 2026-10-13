enable_language(OBJCXX)
find_package(SDL2 REQUIRED)

add_executable(cubey-sim main.cpp hal_mac.mm)
target_link_libraries(cubey-sim PRIVATE cubey_core SDL2::SDL2 "-framework Foundation" "-framework AVFoundation")
target_compile_options(cubey-sim PRIVATE -Wall -Wextra $<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>)
