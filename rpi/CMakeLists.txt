cmake_minimum_required(VERSION 3.16)

project(rc-boat C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")
set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG")
set(CMAKE_C_FLAGS_DEBUG "-O0 -g -DDEBUG")

find_package(Threads REQUIRED)

# Shared with the Pico
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
target_link_libraries(log PUBLIC log_src Threads::Threads)

add_library(gpio lib/gpio/gpio.c)
target_include_directories(gpio PUBLIC lib/gpio ${CONF_CONFIG_DIR})
target_link_libraries(gpio PUBLIC log)

add_library(spi lib/spi/spi.c)
target_include_directories(spi PUBLIC lib/spi ${CONF_CONFIG_DIR})
target_link_libraries(spi PUBLIC log)

add_library(lora lib/lora/lora.c)
target_include_directories(lora PUBLIC lib/lora ${CONF_CONFIG_DIR})
target_link_libraries(lora PUBLIC spi gpio log)

add_library(uart lib/uart/uart.c)
target_include_directories(uart PUBLIC lib/uart ${CONF_CONFIG_DIR})
target_link_libraries(uart PUBLIC log)

# Executable
add_executable(rc-boat main.c)
target_include_directories(rc-boat PRIVATE ${CONF_CONFIG_DIR})
target_link_libraries(rc-boat lora spi gpio uart comm log m Threads::Threads)
