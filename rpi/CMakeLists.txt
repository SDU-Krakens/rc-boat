cmake_minimum_required(VERSION 3.16)

project(rc-boat C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")
set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG")
set(CMAKE_C_FLAGS_DEBUG "-O0 -g -DDEBUG")

# Libraries
add_library(gpio lib/gpio/gpio.c)
target_include_directories(gpio PUBLIC lib/gpio ${CONF_CONFIG_DIR})

add_library(spi lib/spi/spi.c)
target_include_directories(spi PUBLIC lib/spi ${CONF_CONFIG_DIR})

add_library(lora lib/lora/lora.c)
target_include_directories(lora PUBLIC lib/lora ${CONF_CONFIG_DIR})
target_link_libraries(lora PUBLIC spi gpio)

# Executable
add_executable(rc-boat main.c)
target_include_directories(rc-boat PRIVATE ${CONF_CONFIG_DIR})
target_link_libraries(rc-boat lora spi gpio m)
