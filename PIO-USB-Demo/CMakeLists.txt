cmake_minimum_required(VERSION 3.13)
include(pico_sdk_import.cmake)

set(target_name pico_usb_host_msc_demo)
project(${target_name} C CXX ASM)
pico_sdk_init()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DPICO_USE_MALLOC_MUTEX=1")
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)


add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../lib/rp2040_rtc ${CMAKE_CURRENT_LIST_DIR}/build/rp2040_rtc)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../lib/fatfs/source ${CMAKE_CURRENT_LIST_DIR}/build/fatfs/src)

add_executable(${target_name})

set(PICO_PIO_USB_SRC "${PICO_TINYUSB_PATH}/hw/mcu/raspberry_pi/Pico-PIO-USB/src")

pico_generate_pio_header(${target_name} ${PICO_PIO_USB_SRC}/usb_tx.pio)
pico_generate_pio_header(${target_name} ${PICO_PIO_USB_SRC}/usb_rx.pio)
pico_enable_stdio_uart(${target_name} 1)
target_sources(${target_name} PRIVATE
    pico-usb-host-msc-demo.c
    ${CMAKE_CURRENT_LIST_DIR}/../msc-demo-cli.cpp
    ${PICO_PIO_USB_SRC}/pio_usb.c
    ${PICO_PIO_USB_SRC}/pio_usb_device.c
    ${PICO_PIO_USB_SRC}/pio_usb_host.c
    ${PICO_PIO_USB_SRC}/usb_crc.c
    ${CMAKE_CURRENT_LIST_DIR}/../lib/embedded-cli/lib/src/embedded_cli.c
)



target_include_directories(${target_name} PRIVATE
    ${PICO_PIO_USB_SRC}
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/..
    ${CMAKE_CURRENT_LIST_DIR}/../lib/embedded-cli/lib/include
)

target_link_options(${target_name} PRIVATE -Xlinker --print-memory-usage)
target_compile_options(${target_name} PRIVATE -Wall -Wextra -DCFG_TUSB_DEBUG=1)
#target_compile_options(${target_name} PRIVATE -Wall -Wextra)
target_compile_definitions(${target_name} PRIVATE
PICO_DEFAULT_UART_TX_PIN=16
PICO_DEFAULT_UART_RX_PIN=17
)
target_link_libraries(${target_name} pico_stdlib pico_multicore hardware_pio hardware_dma tinyusb_device tinyusb_host rp2040_rtc msc_fatfs)

pico_add_extra_outputs(${target_name})

