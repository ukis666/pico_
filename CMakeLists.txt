cmake_minimum_required(VERSION 3.13)

# Import the Pico SDK
include(pico_sdk_import.cmake)

project(pico_rfid_dfplayer C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Initialize the SDK
pico_sdk_init()

add_executable(rfid_dfplayer
    src/main.cpp
)

target_include_directories(rfid_dfplayer PRIVATE include)

target_link_libraries(rfid_dfplayer
    pico_stdlib
    hardware_uart
)

# Use USB for stdio; keep UART free for peripherals
pico_enable_stdio_usb(rfid_dfplayer 1)
pico_enable_stdio_uart(rfid_dfplayer 0)

# Create .uf2, .elf, map files etc
pico_add_extra_outputs(rfid_dfplayer)

