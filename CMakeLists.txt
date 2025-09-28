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

# GPS
add_library(gps gps/gps.c)
target_include_directories(gps PUBLIC gps)

# IMU
add_library(imu imu/imu.c)
target_include_directories(imu PUBLIC imu)

target_link_libraries(rc-boat gps imu)

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
