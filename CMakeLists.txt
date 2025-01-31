cmake_minimum_required(VERSION 3.13)

if (DEFINED ENV{RPPICOMIDI_PIO_HOST})
set(RPPICOMIDI_PIO_HOST $ENV{RPPICOMIDI_PIO_HOST})
endif()
if (NOT DEFINED RPPICOMIDI_PIO_HOST OR (RPPICOMIDI_PIO_HOST EQUAL 0))
set(BOARD pico_sdk)
endif()

include(pico_sdk_import.cmake)
project(pico_usb_host_msc_demo C CXX ASM)

if(DEFINED RPPICOMIDI_PIO_HOST AND (RPPICOMIDI_PIO_HOST EQUAL 1))
message(STATUS "Compiling for PIO USB Host")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DPICO_USE_MALLOC_MUTEX=1")
else()
message(STATUS "Compiling for RP2040 native USB Host")
endif()
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/lib/rp2040_rtc)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/lib/fatfs/source)
add_executable(pico_usb_host_msc_demo)

if(DEFINED RPPICOMIDI_PIO_HOST AND (RPPICOMIDI_PIO_HOST EQUAL 1))
    set(PICO_PIO_USB_SRC "${PICO_TINYUSB_PATH}/hw/mcu/raspberry_pi/Pico-PIO-USB/src")
    pico_generate_pio_header(pico_usb_host_msc_demo ${PICO_PIO_USB_SRC}/usb_tx.pio)
    pico_generate_pio_header(pico_usb_host_msc_demo ${PICO_PIO_USB_SRC}/usb_rx.pio)
endif()

target_sources(pico_usb_host_msc_demo PRIVATE
    pico-usb-host-msc-demo.c
    msc-demo-cli.cpp
    ${CMAKE_CURRENT_LIST_DIR}/lib/embedded-cli/lib/src/embedded_cli.c
)
if(DEFINED RPPICOMIDI_PIO_HOST AND (RPPICOMIDI_PIO_HOST EQUAL 1))
    target_sources(pico_usb_host_msc_demo PRIVATE
        ${PICO_PIO_USB_SRC}/pio_usb.c
        ${PICO_PIO_USB_SRC}/pio_usb_device.c
        ${PICO_PIO_USB_SRC}/pio_usb_host.c
        ${PICO_PIO_USB_SRC}/usb_crc.c
    )
endif()
pico_enable_stdio_uart(pico_usb_host_msc_demo 1)

target_include_directories(pico_usb_host_msc_demo PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/lib/embedded-cli/lib/include
)
if(DEFINED RPPICOMIDI_PIO_HOST AND (RPPICOMIDI_PIO_HOST EQUAL 1))
    target_include_directories(pico_usb_host_msc_demo PRIVATE
        ${PICO_PIO_USB_SRC}
    )
endif()
target_link_options(pico_usb_host_msc_demo PRIVATE -Xlinker --print-memory-usage)
target_compile_options(pico_usb_host_msc_demo PRIVATE -Wall -Wextra)
if(DEFINED RPPICOMIDI_PIO_HOST AND (RPPICOMIDI_PIO_HOST EQUAL 1))
target_compile_definitions(pico_usb_host_msc_demo PRIVATE
    CFG_TUH_RPI_PIO_USB=1
    BOARD_TUH_RHPORT=1
)
endif()
target_link_libraries(pico_usb_host_msc_demo tinyusb_host tinyusb_board rp2040_rtc msc_fatfs pico_stdlib)
if(DEFINED RPPICOMIDI_PIO_HOST AND (RPPICOMIDI_PIO_HOST EQUAL 1))
    target_link_libraries(pico_usb_host_msc_demo pico_multicore hardware_pio hardware_dma )
endif()
if(DEFINED PICO_BOARD)
    if(${PICO_BOARD} MATCHES "pico_w")
        message("board is pico_w")
        target_compile_options(pico_usb_host_msc_demo PRIVATE -DRPPICOMIDI_PICO_W)
        target_link_libraries(pico_usb_host_msc_demo pico_cyw43_arch_none)
    else()
        message("board is pico")
    endif()
else()
    message("board is not defined")
endif()
pico_add_extra_outputs(pico_usb_host_msc_demo)

