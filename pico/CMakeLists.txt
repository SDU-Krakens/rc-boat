cmake_minimum_required(VERSION 3.16)

set(PICO_BOARD pico_w)
include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(pico C CXX ASM)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")
set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG")
set(CMAKE_C_FLAGS_DEBUG "-O0 -g -DDEBUG")

pico_sdk_init()

# can2040 (software CAN on PIO), no CMakeLists upstream: fetch sources only
include(FetchContent)
FetchContent_Declare(can2040
  GIT_REPOSITORY https://github.com/KevinOConnor/can2040.git
  GIT_TAG master
  SOURCE_SUBDIR no-cmake)
FetchContent_MakeAvailable(can2040)

add_library(can2040 ${can2040_SOURCE_DIR}/src/can2040.c)
target_include_directories(can2040 PUBLIC ${can2040_SOURCE_DIR}/src)
target_link_libraries(can2040 PUBLIC hardware_pio hardware_dma)

# Shared with the Zero
set(COMMON_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../common)

add_library(types INTERFACE)
target_include_directories(types INTERFACE ${COMMON_DIR}/types)

add_library(log_src ${COMMON_DIR}/log/log_src.c)
target_include_directories(log_src PUBLIC ${COMMON_DIR}/log)

add_library(comm ${COMMON_DIR}/comm/comm.c)
target_include_directories(comm PUBLIC ${COMMON_DIR}/comm)
target_link_libraries(comm PUBLIC types log_src)

# Libraries
add_library(log lib/log/log.c)
target_include_directories(log PUBLIC lib/log ${CONF_CONFIG_DIR})
target_link_libraries(log PUBLIC pico_stdlib comm log_src)

add_library(i2c lib/i2c/i2c.c)
target_include_directories(i2c PUBLIC lib/i2c ${CONF_CONFIG_DIR})
target_link_libraries(i2c PUBLIC pico_stdlib hardware_i2c log)

add_library(uart lib/uart/uart.c)
target_include_directories(uart PUBLIC lib/uart ${CONF_CONFIG_DIR})
target_link_libraries(uart PUBLIC pico_stdlib hardware_uart log)

add_library(storage lib/storage/storage.c)
target_include_directories(storage PUBLIC lib/storage ${CONF_CONFIG_DIR})
target_link_libraries(storage PUBLIC pico_stdlib hardware_flash pico_flash log)

add_library(imu
  lib/imu/imu.c
  lib/imu/icm20948.c
  lib/imu/bno055.c
  lib/imu/calib.c
  lib/imu/madgwick.c)
target_include_directories(imu PUBLIC lib/imu ${CONF_CONFIG_DIR})
target_link_libraries(imu PUBLIC pico_stdlib i2c storage types log m)

add_library(gps lib/gps/gps.c lib/gps/ubx.c)
target_include_directories(gps PUBLIC lib/gps ${CONF_CONFIG_DIR})
target_link_libraries(gps PUBLIC pico_stdlib uart types log)

add_library(can lib/can/can.c)
target_include_directories(can PUBLIC lib/can ${CONF_CONFIG_DIR})
target_link_libraries(can PUBLIC pico_stdlib hardware_irq hardware_clocks can2040 types log)

# Executable
add_executable(pico main.c)
target_include_directories(pico PRIVATE ${CONF_CONFIG_DIR})
target_link_libraries(pico pico_stdlib pico_cyw43_arch_none hardware_watchdog i2c uart imu gps can storage comm log)

pico_enable_stdio_usb(pico 0)
pico_enable_stdio_uart(pico 1)

# Keep the last CONF_STORAGE_SIZE bytes of flash out of the firmware image;
# storage.c uses them for calibration data
math(EXPR PICO_APP_FLASH_SIZE "(${PICO_FLASH_SIZE_BYTES}) - ${CONF_STORAGE_SIZE}")
pico_override_flash_size(pico ${PICO_APP_FLASH_SIZE})

pico_add_extra_outputs(pico)
