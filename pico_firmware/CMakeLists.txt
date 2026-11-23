# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.3.1)
set(toolchainVersion 15_2_Rel1)
set(picotoolVersion 2.3.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico CACHE STRING "Board type")

cmake_minimum_required(VERSION 3.13)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(pico_chess_engine C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

# Avoid cmd.exe '&' errors in post-build disassembly commands
set(PICO_NO_TARGET_DISASSEMBLY 1)

add_executable(pico_chess_engine
    pico_main.c
    motor.c
    magnet.c
    ../engine_c/chess_engine.c
)

target_include_directories(pico_chess_engine PRIVATE
    ../engine_c
)

target_link_libraries(pico_chess_engine
    pico_stdlib
)

# Route printf/getchar over the USB cable instead of UART pins
pico_enable_stdio_usb(pico_chess_engine 1)
pico_enable_stdio_uart(pico_chess_engine 0)

# Generate pico_chess_engine.uf2 to drag onto the Pico
pico_add_extra_outputs(pico_chess_engine)

add_executable(magnet_solo magnet_solo.c)
target_link_libraries(magnet_solo pico_stdlib)
pico_add_extra_outputs(magnet_solo)