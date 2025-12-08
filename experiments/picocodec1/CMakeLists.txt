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
set(PICO_BOARD pico2 CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(picocodec1 C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(picocodec1 picocodec1.cpp )

pico_set_program_name(picocodec1 "picocodec1")
pico_set_program_version(picocodec1 "0.1")

# Generate PIO header
pico_generate_pio_header(picocodec1 ${CMAKE_CURRENT_LIST_DIR}/audio_i2s_mclk.pio)

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(picocodec1 1)
pico_enable_stdio_usb(picocodec1 0)

# Add the standard library to the build
target_link_libraries(picocodec1
        pico_stdlib)

# Add the standard include files to the build
target_include_directories(picocodec1 PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)

# Add any user requested libraries
target_link_libraries(picocodec1 
        hardware_pio
        hardware_clocks
        hardware_i2c
        )

pico_add_extra_outputs(picocodec1)

