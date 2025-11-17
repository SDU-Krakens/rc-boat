cmake_minimum_required(VERSION 3.10)

project(rc-boat)

set(CMAKE_C_STANDARD 17)
set(CMAKE_STANDARD_REQUIRED ON)

# For building
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -O2 -DNDEBUG")

# For debugging
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g -DDEBUG")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Executable
add_executable(rc-boat main.c)

# Libraries

# GPS + UART
add_library(gps lib/gps/gps.c)
target_include_directories(gps PUBLIC lib/gps)
add_library(uart lib/uart/uart.c)
target_include_directories(uart PUBLIC lib/uart)

# IMU
add_library(mpu6050 lib/imu/mpu6050.c)
target_include_directories(mpu6050 PUBLIC lib/imu)

# LoRa + SPI
add_library(lora lib/lora/lora.c)
target_include_directories(lora PUBLIC lib/lora)
add_library(spi lib/spi/spi.c)
target_include_directories(spi PUBLIC lib/spi)

# GPIO
add_library(gpio lib/gpio/gpio.c)
target_include_directories(gpio PUBLIC lib/gpio)

target_link_libraries(rc-boat gps uart mpu6050 lora spi gpio)

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
