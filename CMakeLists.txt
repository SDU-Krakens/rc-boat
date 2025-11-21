cmake_minimum_required(VERSION 3.10)

project(rc-boat)

set(CMAKE_C_STANDARD 17)
set(CMAKE_STANDARD_REQUIRED ON)

# For building
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -O2 -DNDEBUG")

# For debugging
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g -DDEBUG")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

option(BUILD_TESTS "Build tests" OFF)

# Main exe
add_executable(rc-boat main.c)

### Libraries

# Unity
add_library(unity lib/unity/src/unity.c)
target_include_directories(unity PUBLIC lib/unity/src)

# GPS + UART
add_library(gps lib/gps/gps.c)
target_include_directories(gps PUBLIC lib/gps)
add_library(nmea lib/gps/nmea.c)
target_include_directories(nmea PUBLIC lib/gps)
add_library(uart lib/uart/uart.c)
target_include_directories(uart PUBLIC lib/uart)

# IMU + I2C
add_library(mpu lib/imu/mpu6050.c)
target_include_directories(mpu PUBLIC lib/imu)
add_library(i2c lib/i2c/i2c.c)
target_include_directories(i2c PUBLIC lib/i2c)

# LoRa + SPI
add_library(lora lib/lora/lora.c)
target_include_directories(lora PUBLIC lib/lora)
add_library(spi lib/spi/spi.c)
target_include_directories(spi PUBLIC lib/spi)

# GPIO
add_library(gpio lib/gpio/gpio.c)
target_include_directories(gpio PUBLIC lib/gpio)

target_link_libraries(rc-boat gps uart mpu lora spi gpio i2c nmea)

find_library(RT_LIBRARY rt)
find_library(M_LIBRARY m)

# Find libraries
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


if(BUILD_TESTS)
  enable_testing()
  file(GLOB TEST_FILES "${CMAKE_CURRENT_SOURCE_DIR}/test/test_*.c")
  foreach(test_file ${TEST_FILES})
    get_filename_component(test_name ${test_file} NAME_WE)
    add_executable(${test_name} ${test_file})
    target_link_libraries(${test_name} unity nmea gps uart mpu lora spi i2c gpio m rt)
    add_test(NAME ${test_name} COMMAND $<TARGET_FILE:${test_name}>)
  endforeach()
endif()

# We don't need this yet
# install(TARGETS rc-boat DESTINATION bin)
