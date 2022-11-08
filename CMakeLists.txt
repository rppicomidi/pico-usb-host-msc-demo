cmake_minimum_required(VERSION 3.13)

set(BOARD pico_sdk)
include(pico_sdk_import.cmake)

set(target pico_usb_host_msc_demo)
project(${target} C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/lib/fatfs)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/lib/rp2040_rtc)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/lib/fatfs/source)

add_executable(${target}
    pico-usb-host-msc-demo.c
    msc-demo-cli.cpp
    ${CMAKE_CURRENT_LIST_DIR}/lib/embedded-cli/lib/src/embedded_cli.c
)

pico_enable_stdio_uart(${target} 1)

target_include_directories(
    ${target} PRIVATE ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/lib/embedded-cli/lib/include
)
target_link_options(${target} PRIVATE -Xlinker --print-memory-usage)
target_compile_options(${target} PRIVATE -Wall -Wextra)
target_link_libraries(${target} tinyusb_host tinyusb_board rp2040_rtc msc_fatfs pico_stdlib)

pico_add_extra_outputs(${target})

