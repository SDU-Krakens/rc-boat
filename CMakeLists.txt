cmake_minimum_required(VERSION 3.16)

# Orchestrates two sub-builds with different toolchains:
#   pico/ -> Pico SDK (arm-none-eabi), dist/pico
#   rpi/  -> Zero 2 W (aarch64 Linux), dist/rpi
project(rc-boat NONE)

include(ExternalProject)

# ---------------------------------------------------------------------------
# Configuration (edit with `make config`)
# ---------------------------------------------------------------------------
function(conf NAME TYPE DEFAULT HELP)
  set(${NAME} "${DEFAULT}" CACHE ${TYPE} "${HELP}")
endfunction()

# Pico <-> Zero link
conf(CONF_LINK_UART STRING 0 "Pico UART instance for the Zero link")
conf(CONF_LINK_TX_PIN STRING 0 "Pico link TX GPIO")
conf(CONF_LINK_RX_PIN STRING 1 "Pico link RX GPIO")
conf(CONF_LINK_BAUD STRING 921600 "Pico <-> Zero link baud rate")
conf(CONF_LINK_DEV STRING /dev/serial0 "Zero serial device for the link")
conf(CONF_COMM_RESEND_MS STRING 100 "Resend an unconfirmed text after this many ms")
conf(CONF_PICO_LOG_QUEUE STRING 65536 "Pico log queue bytes before the main loop pauses")

# IMUs
conf(CONF_IMU0_I2C STRING 0 "I2C instance of IMU0")
conf(CONF_IMU0_SDA STRING 4 "IMU0 SDA GPIO")
conf(CONF_IMU0_SCL STRING 5 "IMU0 SCL GPIO")
conf(CONF_IMU1_I2C STRING 1 "I2C instance of IMU1")
conf(CONF_IMU1_SDA STRING 2 "IMU1 SDA GPIO")
conf(CONF_IMU1_SCL STRING 3 "IMU1 SCL GPIO")
conf(CONF_I2C_BAUD STRING 400000 "I2C bus speed in Hz")
option(CONF_I2C_PULLUPS "Enable RP2040 internal I2C pull-ups" ON)
conf(CONF_IMU_SAMPLE_HZ STRING 100 "IMU sample rate in Hz")
conf(CONF_ICM_ACCEL_RANGE_G STRING 2 "ICM-20948 accel range: 2, 4, 8 or 16 g")
conf(CONF_ICM_GYRO_RANGE_DPS STRING 250 "ICM-20948 gyro range: 250, 500, 1000 or 2000 dps")
conf(CONF_ICM_DLPF STRING 3 "ICM-20948 DLPF index 0-7 (3 = ~51 Hz gyro / ~50 Hz accel)")
conf(CONF_MADGWICK_BETA STRING 0.1f "Madgwick filter gain")
conf(CONF_GYRO_CAL_SAMPLES STRING 500 "Samples for gyro bias calibration")
conf(CONF_STORAGE_SIZE STRING 4096 "Reserved flash for calibration, multiple of 4096")

# GPS
conf(CONF_GPS_UART STRING 1 "Pico UART instance for the GPS")
conf(CONF_GPS_TX_PIN STRING 8 "Pico GPS TX GPIO")
conf(CONF_GPS_RX_PIN STRING 9 "Pico GPS RX GPIO")
conf(CONF_GPS_BAUD STRING 115200 "GPS baud rate after configuration")
conf(CONF_GPS_RATE_HZ STRING 5 "GPS navigation rate in Hz")
conf(CONF_GPS_CFG_RETRIES STRING 3 "GPS configuration attempts")
conf(CONF_GPS_ACK_TIMEOUT_MS STRING 500 "GPS ACK timeout in ms")

# CAN
conf(CONF_CAN_PIO STRING 0 "PIO block used by can2040")
conf(CONF_CAN_RX_PIN STRING 15 "CAN RX GPIO")
conf(CONF_CAN_TX_PIN STRING 13 "CAN TX GPIO")
conf(CONF_CAN_BITRATE STRING 1000000 "CAN bitrate")
conf(CONF_CAN_RX_BUF STRING 32 "CAN receive buffer in frames")

# Zero: LoRa
conf(CONF_GPIO_CHIP STRING /dev/gpiochip0 "Zero GPIO character device")
conf(CONF_LORA_SPI_DEV STRING /dev/spidev0.0 "Zero spidev for the SX1278")
conf(CONF_LORA_SPI_HZ STRING 1000000 "SX1278 SPI clock in Hz")
conf(CONF_LORA_RESET_PIN STRING 26 "SX1278 RESET GPIO")
conf(CONF_LORA_DIO0_PIN STRING 22 "SX1278 DIO0 GPIO")
conf(CONF_LORA_FREQUENCY STRING 433000000 "LoRa frequency in Hz")
conf(CONF_LORA_BANDWIDTH STRING 250000 "LoRa bandwidth in Hz")
conf(CONF_LORA_SF STRING 7 "LoRa spreading factor 6-12")
conf(CONF_LORA_CR STRING 5 "LoRa coding rate denominator 5-8")
conf(CONF_LORA_TX_POWER STRING 17 "LoRa TX power in dBm")
conf(CONF_LORA_PREAMBLE STRING 8 "LoRa preamble length")
option(CONF_LORA_CRC "LoRa payload CRC" ON)
conf(CONF_LORA_LDRO STRING AUTO "LoRa low data rate optimize: AUTO, OFF or ON")
set_property(CACHE CONF_LORA_LDRO PROPERTY STRINGS AUTO OFF ON)
conf(CONF_LORA_TX_TIMEOUT_MS STRING 500 "LoRa TX timeout in ms")
conf(CONF_LORA_TELEMETRY_BURST STRING 5 "Telemetry packets before a log window")
conf(CONF_LORA_LOG_WINDOW_MS STRING 500 "Longest time for log lines between telemetry bursts")

# Zero: command socket
conf(CONF_CMD_SOCKET STRING /tmp/rc-boat.sock "Unix socket for cmd.py")
conf(CONF_CMD_REPLY_TIMEOUT_MS STRING 2000 "Wait this long for a Pico reply")

# Zero: logging
conf(CONF_LOG_DIR STRING /var/log/rc-boat "Directory for log files (must exist)")
conf(CONF_LOG_LORA_QUEUE STRING 64 "Log lines waiting for LoRa")
conf(CONF_LOG_LINE_MAX STRING 512 "Maximum log line length in bytes")
conf(CONF_LOG_PATH_MAX STRING 256 "Maximum log file path length")
conf(CONF_LOG_FLUSH_MS STRING 1000 "Flush the log file at most this often")

# Zero: SWD flashing of the Pico
conf(CONF_SWD_SWDIO_PIN STRING 24 "Zero GPIO wired to Pico SWDIO")
conf(CONF_SWD_SWCLK_PIN STRING 25 "Zero GPIO wired to Pico SWCLK")

set(CONF_CONFIG_DIR ${CMAKE_BINARY_DIR}/config)
configure_file(${CMAKE_SOURCE_DIR}/config.h.in ${CONF_CONFIG_DIR}/config.h)
configure_file(${CMAKE_SOURCE_DIR}/config.mk.in ${CONF_CONFIG_DIR}/config.mk)

# Every CONF_* value is passed down to both sub-builds
set(CONF_ARGS -DCONF_CONFIG_DIR=${CONF_CONFIG_DIR})
get_cmake_property(_vars CACHE_VARIABLES)
foreach(_var IN LISTS _vars)
  if(_var MATCHES "^CONF_")
    list(APPEND CONF_ARGS "-D${_var}=${${_var}}")
  endif()
endforeach()

set(SUB_ARGS
  -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
  ${CONF_ARGS})

# ---------------------------------------------------------------------------
# Pico
# ---------------------------------------------------------------------------
if(NOT DEFINED ENV{PICO_SDK_PATH})
  message(FATAL_ERROR "PICO_SDK_PATH is not set")
endif()

ExternalProject_Add(pico
  SOURCE_DIR ${CMAKE_SOURCE_DIR}/pico
  BINARY_DIR ${CMAKE_BINARY_DIR}/pico
  CMAKE_ARGS ${SUB_ARGS} -DPICO_SDK_PATH=$ENV{PICO_SDK_PATH}
  BUILD_ALWAYS TRUE
  INSTALL_COMMAND "")

# ---------------------------------------------------------------------------
# Zero 2 W (cross compiled unless we are on the Zero itself)
# ---------------------------------------------------------------------------
set(RPI_ARGS ${SUB_ARGS})
if(NOT CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
  list(APPEND RPI_ARGS
    -DCMAKE_TOOLCHAIN_FILE=${CMAKE_SOURCE_DIR}/rpi/cmake/aarch64-linux-gnu.cmake)
endif()

ExternalProject_Add(rpi
  SOURCE_DIR ${CMAKE_SOURCE_DIR}/rpi
  BINARY_DIR ${CMAKE_BINARY_DIR}/rpi
  CMAKE_ARGS ${RPI_ARGS}
  BUILD_ALWAYS TRUE
  INSTALL_COMMAND "")

# compile_commands.json next to each source tree for clangd
foreach(_sub pico rpi)
  ExternalProject_Add_Step(${_sub} compile_commands
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      ${CMAKE_BINARY_DIR}/${_sub}/compile_commands.json
      ${CMAKE_SOURCE_DIR}/${_sub}/compile_commands.json
    DEPENDEES configure
    ALWAYS TRUE)
endforeach()
