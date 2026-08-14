# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD_HEADER_DIRS ${CMAKE_SOURCE_DIR})
set(PICO_BOARD rp2350_bellsnwhistles CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(uxn_rpi C CXX ASM)

pico_sdk_init()

add_subdirectory(${CMAKE_SOURCE_DIR}/Libraries/pico-fatfs-sd/src build)
add_subdirectory(${CMAKE_SOURCE_DIR}/Libraries/pico_hdmi)

add_executable(uxn_rpi uxn_rpi.c core1.c uxn/uxn11.c uxn/devices/file.c uxn/devices/datetime.c uxn/devices/screen.c uxn/devices/system.c uxn/devices/mouse.c uxn/devices/controller.c uxn/devices/console.c ps2/ps2_shared.c hw_config.c
Libraries/pico-ds1302/ds1302.c)

pico_set_program_name(uxn_rpi "uxn_rpi")
pico_set_program_version(uxn_rpi "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(uxn_rpi 1)
pico_enable_stdio_usb(uxn_rpi 0)
pico_enable_stdio_rtt(uxn_rpi 0)

pico_generate_pio_header(uxn_rpi ${CMAKE_CURRENT_LIST_DIR}/ps2/ps2_input.pio)

target_link_libraries(uxn_rpi
        pico_stdlib hardware_dma pico_multicore pico_sync hardware_i2c
        pico-fatfs-sd pico_hdmi)

target_include_directories(uxn_rpi PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/uxn
        ${CMAKE_CURRENT_LIST_DIR}/uxn/uxn/src
        ${CMAKE_CURRENT_LIST_DIR}/ps2
        ${CMAKE_CURRENT_LIST_DIR}/Libraries/pico-ds1302
)

pico_set_linker_script(uxn_rpi ${CMAKE_SOURCE_DIR}/linkerfile.ld)

pico_add_extra_outputs(uxn_rpi)
set_property(TARGET uxn_rpi APPEND_STRING PROPERTY LINK_FLAGS "-Wl,--print-memory-usage")