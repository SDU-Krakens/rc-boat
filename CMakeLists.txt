cmake_minimum_required(VERSION 3.10)

set(PICO_BOARD pico_w)
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(rc-boat)

set(CMAKE_C_STANDARD 17)
set(CMAKE_STANDARD_REQUIRED ON)

# For building
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -O2 -DNDEBUG")

# For debugging
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g -DDEBUG")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Pico stuff
pico_sdk_init()

set(sdkVersion 2.3.1)
set(toolchainVersion 15_2_Rel1)
set(picotoolVersion 2.3.1)

# Executable
add_executable(rc-boat rpi/main.c)
add_executable(pico pico/main.c)

# Libraries rc-boat

# GPS + UART
add_library(gps rpi/lib/gps/gps.c)
target_include_directories(gps PUBLIC rpi/lib/gps)
add_library(uart rpi/lib/uart/uart.c)
target_include_directories(uart PUBLIC rpi/lib/uart)
add_library(nmea rpi/lib/gps/nmea.c)
target_include_directories(nmea PUBLIC rpi/lib/gps)

# IMU
add_library(mpu6050 rpi/lib/imu/mpu6050.c)
target_include_directories(mpu6050 PUBLIC rpi/lib/imu)

# LoRa + SPI
add_library(lora rpi/lib/lora/lora.c)
target_include_directories(lora PUBLIC rpi/lib/lora)
add_library(spi rpi/lib/spi/spi.c)
target_include_directories(spi PUBLIC rpi/lib/spi)

# GPIO
add_library(gpio rpi/lib/gpio/gpio.c)
target_include_directories(gpio PUBLIC rpi/lib/gpio)

target_link_libraries(rc-boat gps uart mpu6050 lora spi gpio nmea)

# Libraries pico

pico_enable_stdio_usb(pico 0)
pico_enable_stdio_uart(pico 1)

pico_add_extra_outputs(pico)

target_link_libraries(pico pico_stdlib hardware_i2c pico_cyw43_arch_none)


find_library(RT_LIBRARY rt)
find_library(M_LIBRARY m)

if(RT_LIBRARY)
  target_link_libraries(rc-boat ${RT_LIBRARY})
else()
  message(STATUS "rt library not found, assuming it's in libc")
endif()

if(M_LIBRARY)
  target_link_libraries(rc-boat ${M_LIBRARY})
else()
  message(STATUS "math library not found, assuming it's in libc")
endif()

# We don't need this yet
# install(TARGETS rc-boat DESTINATION bin)
