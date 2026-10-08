# RC Boat

This repository contains the firmware for the RC Boat prototype.

This document uses the ASD-STE100 Simplified Technical English rules.

## 1. System description

The system has two computers:

- **Pico**: a Raspberry Pi Pico. It is the sensor hub. The `PICO_BOARD`
  value in `pico/CMakeLists.txt` sets the board type.
- **Zero**: a Raspberry Pi Zero 2 W with 64-bit Raspberry Pi OS. It is the
  main computer.

The Pico reads these sensors:

- Two IMUs on two I2C buses. One IMU is a BNO055. The other IMU is an
  ICM-20948. The Pico finds the IMU type on each bus at start.
- One u-blox GPS receiver on a UART.
- One CAN bus. The Pico uses the can2040 library (software CAN on PIO).

The Pico sends all sensor data to the Zero through a UART link.

The Zero does these tasks:

- It receives the data from the Pico.
- It writes all data and all log lines to a log file.
- It sends telemetry and log lines through an SX1278 LoRa radio.
- It receives requests from `cmd.py` on a Unix socket. It sends these
  requests to the Pico.

```
 IMU0 (I2C) ──┐
 IMU1 (I2C) ──┤                 UART link               SPI
 GPS  (UART) ─┼── Pico ◄─────────────────────► Zero ◄─────────► SX1278 LoRa
 CAN  (PIO) ──┘                                 ▲
                                                │ Unix socket
                                             cmd.py
```

The Zero can also program the Pico through SWD. Two Zero GPIOs connect to
the SWDIO and SWCLK pins of the Pico.

## 2. Directory structure

| Directory          | Contents                                              |
| ------------------ | ----------------------------------------------------- |
| `pico/`            | Pico program (`main.c`) and Pico drivers (`lib/`)     |
| `rpi/`             | Zero program (`main.c`) and Zero drivers (`lib/`)     |
| `common/comm/`     | Link protocol. The Pico and the Zero use the same code |
| `common/log/`      | Log source and severity names for the two computers   |
| `common/types/`    | Data types for the IMU sample, GPS fix and CAN frame  |
| `config.h.in`      | Template for the generated `config.h`                 |
| `config.mk.in`     | Template for the generated `config.mk`                |
| `cmd.py`           | Command client for the Zero socket                    |
| `setup/`           | Zero setup script, `boat` service and `cmd` shell     |
| `schematic.pdf`    | Electrical schematic                                  |
| `dist/`            | Build output. Git ignores this directory              |

### 2.1 Pico drivers

| Driver    | Function                                                       |
| --------- | -------------------------------------------------------------- |
| `i2c`     | I2C bus                                                        |
| `uart`    | UART                                                           |
| `imu`     | BNO055 and ICM-20948, calibration, Madgwick filter             |
| `gps`     | u-blox GPS with the UBX protocol                               |
| `can`     | CAN bus with can2040                                           |
| `storage` | Calibration data and settings in the last sector of the flash  |
| `log`     | Log lines. The Pico sends them to the Zero through the link    |

### 2.2 Zero drivers

| Driver | Function                                    |
| ------ | ------------------------------------------- |
| `gpio` | GPIO through the GPIO character device       |
| `spi`  | SPI through `spidev`                         |
| `lora` | SX1278 LoRa radio                            |
| `uart` | UART link to the Pico                        |
| `log`  | Log file and log queue for LoRa              |

## 3. Requirements

### 3.1 Requirements on the build computer

- CMake 3.16 or higher, and `ccmake` for `make config`.
- GNU Make.
- The Pico SDK. Set the `PICO_SDK_PATH` environment variable to the SDK
  directory. If `PICO_SDK_PATH` is not set, the build stops.
- The `arm-none-eabi` toolchain for the Pico SDK.
- The `aarch64-linux-gnu-gcc` cross compiler. On Arch Linux, install the
  `aarch64-linux-gnu-gcc` package. The build does not use the cross
  compiler on an aarch64 computer (for example, on the Zero).
- Git. The Pico build downloads can2040 from GitHub.
- `ssh` and `scp` for the remote targets.

### 3.2 Requirements on the Zero

- OpenOCD with the `raspberrypi-native` interface. The `flash` target uses
  it.
- Python 3 for `cmd.py`.
- The log directory (default `/var/log/rc-boat`). The directory must exist.
  The Zero program does not make it.
- A systemd service with the name `boat`. The `deploy` and `deploy-remote`
  targets restart this service. The service file is `setup/boat.service`.

The setup script installs all these items. Refer to section 3.3.

### 3.3 Set up the Zero

`setup/setup.sh` does the full setup of the Zero. Use it on a new
Raspberry Pi OS (Bookworm or newer). Use it again if the Zero has a
problem. The script is safe to run again.

1. Connect the Zero to the internet and open a shell on it.
2. Type this command:

   ```
   curl -fsSL https://raw.githubusercontent.com/SDU-Krakens/rc-boat/main/setup/setup.sh | sudo bash
   ```

3. Answer the questions. The script asks for:
   - The repository directory (default `/opt/rc-boat`).
   - The Wi-Fi SSID and passphrase.
   - The password for the `cmd` user.
   - The password for `root`.

   The script asks for secrets two times. The script itself contains no
   secrets.
4. Wait. The script then works without input and restarts the Zero at the
   end.

The script does these steps:

1. Installs the packages: build tools, `arm-none-eabi` toolchain, CMake,
   Python 3 and OpenOCD.
2. Clones the Pico SDK 2.3.1 into `/opt/pico-sdk`. It sets
   `PICO_SDK_PATH` in `/etc/profile.d/pico-sdk.sh`.
3. Clones the `main` branch into the repository directory. If the
   directory exists, it resets the directory to `origin/main`.
4. Builds with `make build`.
5. Makes the log directory `/var/log/rc-boat`.
6. Enables the UART (serial console off) and SPI.
7. Makes the `rc-boat` group and the `cmd` user. The login shell of `cmd`
   is `/usr/local/bin/rc-boat-cmd`. It starts `cmd.py`.
8. Sets the root password. Permits root login with a password over SSH
   (`/etc/ssh/sshd_config.d/rc-boat.conf`).
9. Sets the hostname to `boat`.
10. Installs and enables `boat.service`. The service runs as root with the
    group `rc-boat`.
11. Programs the Pico with `make flash`. If this step fails, the script
    shows a warning and continues.
12. Adds the Wi-Fi connection `rc-boat-wifi` with DNS 1.1.1.1.
13. Restarts the Zero.

> **CAUTION:** The script does `git reset --hard origin/main` in the
> repository directory. This removes all local changes.

> **NOTE:** The script adds the Wi-Fi connection but does not start it.
> The Zero connects after the restart. Thus the current SSH connection
> stays open until the end.

## 4. Build

Use the `Makefile` targets. Do not use `cmake` directly.

### 4.1 Build the firmware

1. Set `PICO_SDK_PATH`.
2. Type `make build`.

The build makes these files:

| File                     | Description                       |
| ------------------------ | --------------------------------- |
| `dist/pico/pico.elf`     | Pico firmware                     |
| `dist/rpi/rc-boat`       | Zero program, linked statically   |
| `dist/config/config.h`   | Configuration for the C code      |
| `dist/config/config.mk`  | Configuration for the `Makefile`  |

The build also copies `compile_commands.json` into `pico/` and `rpi/` for
clangd.

The default build type is `Debug`. To make a release build, type
`make build BUILD_TYPE=Release`.

### 4.2 Change the configuration

1. Type `make config`.
2. Change the `CONF_*` values in `ccmake`.
3. Save and close `ccmake`.
4. Type `make build`.

All `CONF_*` values go to the Pico build and to the Zero build. Section 8
gives the list of values.

## 5. Make targets

| Target          | Run on   | Function                                                                 |
| --------------- | -------- | ------------------------------------------------------------------------ |
| `build`         | Any      | Configures and builds the Pico firmware and the Zero program             |
| `config`        | Any      | Opens `ccmake` to change the configuration                               |
| `pullbuild`     | Any      | Pulls the current branch from `origin`, then builds                      |
| `push`          | Laptop   | Adds all changes, commits them as "fast push", and pushes the branch     |
| `flash`         | Zero     | Programs `dist/pico/pico.elf` into the Pico through SWD                  |
| `flash-remote`  | Laptop   | Builds, copies the Pico firmware to the Zero, and runs `make flash` there |
| `deploy`        | Zero     | Resets to `origin/main`, builds, programs the Pico, restarts `boat`      |
| `deploy-remote` | Laptop   | Builds, copies all output to the Zero, programs the Pico, restarts `boat` |

The remote targets use these variables:

| Variable    | Default                 | Description                         |
| ----------- | ----------------------- | ----------------------------------- |
| `ZERO_HOST` | `root@10.10.4.2`        | SSH user and address of the Zero    |
| `ZERO_DIR`  | `/opt/rc-boat`          | Repository directory on the Zero    |

Example: `make deploy-remote ZERO_HOST=root@192.168.1.20`

The first `ssh` connection asks for the password. The subsequent `ssh` and
`scp` commands of the same target use this connection again.

> **CAUTION:** `make deploy` does `git reset --hard origin/main` on the Zero.
> This removes all local changes in `ZERO_DIR`. Commit or copy your changes
> before you use this target.

> **CAUTION:** `make push` adds all files in the working tree to the commit.
> Make sure that the working tree contains only the changes that you want
> to push.

## 6. Operation

### 6.1 Pico start sequence

1. The Pico opens the link to the Zero. All subsequent log lines go to the
   Zero.
2. The Pico reads the settings from the flash. If there is no saved value,
   it uses the `CONF_*` value.
3. The Pico finds the two IMUs and loads their calibration from the flash.
4. If the ICM-20948 has no saved gyro bias, the Pico starts a gyro
   calibration.
5. The Pico opens the GPS and the CAN bus.
6. The Pico starts the main loop.

> **CAUTION:** Keep the boat still during the gyro calibration. Movement
> gives an incorrect gyro bias.

### 6.2 Pico main loop

The main loop runs at the IMU rate. In each cycle, the Pico does these
steps:

1. It reads the two IMUs and sends the `imu0` and `imu1` samples.
2. It calculates the combined sample and sends it as `imu`. If the two IMUs
   have data, the combined sample is the average of the two samples. A
   Madgwick filter calculates the orientation. If only one IMU has data,
   the combined sample is a copy of that sample.
3. It sends a GPS fix if a new fix is available.
4. It sends all received CAN frames.
5. It reads the link and sends log lines.

### 6.3 Zero program

The Zero program has two threads:

- **Reader thread**: It reads the link and the command socket. It writes
  each message from the Pico to the log file.
- **LoRa thread**: It sends a burst of `CONF_LORA_TELEMETRY_BURST`
  telemetry packets. Then it sends log lines for a maximum of
  `CONF_LORA_LOG_WINDOW_MS`. Then it starts again.

If the Zero program cannot open the log file, it writes an error on the
terminal. It also sends the error as one LoRa log packet. Then the program
stops.

If the Zero program cannot open the link, it stops.

If the Zero program cannot open the LoRa radio, it continues without LoRa.

## 7. Command client (`cmd.py`)

`cmd.py` sends requests to the Zero program through the Unix socket
(default `/tmp/rc-boat.sock`). Use it on the Zero.

To send one request, put the request after the command:

```
./cmd.py cmd gyro cal 500
```

To send more requests, start `cmd.py` without a request. Then type one
request on each line. Type `exit` or `quit` to stop.

```
./cmd.py
rc-boat> set beta 0.1
ok
```

To use a different socket, type `./cmd.py --socket <path>`.

From a different computer, connect with SSH as the `cmd` user. The login
shell of this user is `cmd.py`, so you get the `rc-boat>` prompt
immediately. Type `exit` to disconnect.

```
ssh cmd@<zero address>
```

The socket has the permissions `0660` and the group `rc-boat`. Only root
and the members of `rc-boat` can connect.

Only one client can connect at a time. A second client gets the answer
`error busy`.

### 7.1 Requests

| Request                      | Function                                                         |
| ---------------------------- | ---------------------------------------------------------------- |
| `cmd mag cal begin`          | Starts the magnetometer calibration of the ICM-20948             |
| `cmd mag cal end`            | Stops the magnetometer calibration and calculates the result     |
| `cmd gyro cal [samples]`     | Starts a gyro calibration of the ICM-20948. The default number of samples is `CONF_GYRO_CAL_SAMPLES` |
| `cmd save cal`               | Saves the calibration of the two IMUs in the Pico flash          |
| `cmd reboot`                 | Restarts the Pico                                                |
| `set imu rate <Hz>`          | Sets the IMU rate. Range: 1 to 100 Hz                            |
| `set gps rate <Hz>`          | Sets the GPS rate. Range: 1 to 10 Hz                             |
| `set beta <value>`           | Sets the Madgwick filter gain. Range: more than 0, maximum 1     |
| `can <id> [ext] [rtr] [bytes]` | Sends one CAN frame. Write the ID and the data bytes in hexadecimal. Maximum 8 data bytes |

The Pico saves each `set` value in its flash. The Pico uses the saved value
after a restart.

Example: `can 1A3 ext 01 02 FF` sends an extended frame with the ID 0x1A3
and three data bytes.

### 7.2 Answers

| Answer          | Meaning                                                        |
| --------------- | -------------------------------------------------------------- |
| `ok`            | The Pico did the request                                       |
| `unknown`       | The Pico does not know the command or the setting              |
| `out of range`  | The value is not in the permitted range                        |
| `no device`     | The necessary device is not available (for example, no ICM-20948) |
| `failed`        | The Pico tried the request, but it did not work                |
| `busy`          | A calibration runs, or the CAN transmit queue is full          |
| `timeout`       | The Pico did not answer in `CONF_CMD_REPLY_TIMEOUT_MS`         |
| `error ...`     | The Zero did not accept the request. The text gives the cause  |

The Pico answers `cmd gyro cal` when the calibration is complete. If the
calibration takes more time than `CONF_CMD_REPLY_TIMEOUT_MS`, the answer is
`timeout`. The calibration continues, and the result goes to the log file.

## 8. Configuration values

Type `make config` to change these values. The table gives the default
values.

### 8.1 Pico to Zero link

| Value                 | Default        | Description                                          |
| --------------------- | -------------- | ---------------------------------------------------- |
| `CONF_LINK_UART`      | 0              | Pico UART for the link                               |
| `CONF_LINK_TX_PIN`    | 0              | Pico TX GPIO                                         |
| `CONF_LINK_RX_PIN`    | 1              | Pico RX GPIO                                         |
| `CONF_LINK_BAUD`      | 921600         | Link baud rate                                       |
| `CONF_LINK_DEV`       | `/dev/serial0` | Zero serial device                                   |
| `CONF_COMM_RESEND_MS` | 100            | The Pico sends a log line again after this time if the Zero does not confirm it |
| `CONF_PICO_LOG_QUEUE` | 65536          | Bytes of log lines in the Pico queue. When the queue is full, the main loop stops until the Zero confirms the lines |

### 8.2 IMUs

| Value                     | Default | Description                                    |
| ------------------------- | ------- | ---------------------------------------------- |
| `CONF_IMU0_I2C`           | 0       | I2C bus of IMU0                                |
| `CONF_IMU0_SDA`           | 4       | IMU0 SDA GPIO                                  |
| `CONF_IMU0_SCL`           | 5       | IMU0 SCL GPIO                                  |
| `CONF_IMU1_I2C`           | 1       | I2C bus of IMU1                                |
| `CONF_IMU1_SDA`           | 2       | IMU1 SDA GPIO                                  |
| `CONF_IMU1_SCL`           | 3       | IMU1 SCL GPIO                                  |
| `CONF_I2C_BAUD`           | 400000  | I2C speed in Hz                                |
| `CONF_I2C_PULLUPS`        | ON      | Internal I2C pull-up resistors of the Pico     |
| `CONF_IMU_SAMPLE_HZ`      | 100     | IMU rate in Hz                                 |
| `CONF_ICM_ACCEL_RANGE_G`  | 2       | ICM-20948 accelerometer range: 2, 4, 8 or 16 g |
| `CONF_ICM_GYRO_RANGE_DPS` | 250     | ICM-20948 gyro range: 250, 500, 1000 or 2000 dps |
| `CONF_ICM_DLPF`           | 3       | ICM-20948 low-pass filter, 0 to 7              |
| `CONF_MADGWICK_BETA`      | 0.1f    | Madgwick filter gain                           |
| `CONF_GYRO_CAL_SAMPLES`   | 500     | Samples for a gyro calibration                 |
| `CONF_STORAGE_SIZE`       | 4096    | Flash for calibration and settings. Use a multiple of 4096 |

### 8.3 GPS

| Value                     | Default | Description                       |
| ------------------------- | ------- | --------------------------------- |
| `CONF_GPS_UART`           | 1       | Pico UART for the GPS             |
| `CONF_GPS_TX_PIN`         | 8       | Pico GPS TX GPIO                  |
| `CONF_GPS_RX_PIN`         | 9       | Pico GPS RX GPIO                  |
| `CONF_GPS_BAUD`           | 115200  | GPS baud rate after configuration |
| `CONF_GPS_RATE_HZ`        | 5       | GPS rate in Hz                    |
| `CONF_GPS_CFG_RETRIES`    | 3       | Number of configuration attempts  |
| `CONF_GPS_ACK_TIMEOUT_MS` | 500     | GPS ACK timeout in ms             |

### 8.4 CAN

| Value              | Default | Description                  |
| ------------------ | ------- | ---------------------------- |
| `CONF_CAN_PIO`     | 0       | PIO block for can2040        |
| `CONF_CAN_RX_PIN`  | 15      | CAN RX GPIO                  |
| `CONF_CAN_TX_PIN`  | 13      | CAN TX GPIO                  |
| `CONF_CAN_BITRATE` | 1000000 | CAN bit rate                 |
| `CONF_CAN_RX_BUF`  | 32      | Receive buffer, in frames    |

### 8.5 Zero LoRa

| Value                        | Default          | Description                               |
| ---------------------------- | ---------------- | ----------------------------------------- |
| `CONF_GPIO_CHIP`             | `/dev/gpiochip0` | Zero GPIO character device                |
| `CONF_LORA_SPI_DEV`          | `/dev/spidev0.0` | SPI device for the SX1278                 |
| `CONF_LORA_SPI_HZ`           | 1000000          | SPI clock in Hz                           |
| `CONF_LORA_RESET_PIN`        | 26               | SX1278 RESET GPIO                         |
| `CONF_LORA_DIO0_PIN`         | 22               | SX1278 DIO0 GPIO                          |
| `CONF_LORA_FREQUENCY`        | 433000000        | Frequency in Hz                           |
| `CONF_LORA_BANDWIDTH`        | 250000           | Bandwidth in Hz                           |
| `CONF_LORA_SF`               | 7                | Spreading factor, 6 to 12                 |
| `CONF_LORA_CR`               | 5                | Coding rate denominator, 5 to 8           |
| `CONF_LORA_TX_POWER`         | 17               | Transmit power in dBm                     |
| `CONF_LORA_PREAMBLE`         | 8                | Preamble length                           |
| `CONF_LORA_CRC`              | ON               | Payload CRC                               |
| `CONF_LORA_LDRO`             | AUTO             | Low data rate optimization: AUTO, OFF or ON |
| `CONF_LORA_TX_TIMEOUT_MS`    | 500              | Transmit timeout in ms                    |
| `CONF_LORA_TELEMETRY_BURST`  | 5                | Telemetry packets before the log lines    |
| `CONF_LORA_LOG_WINDOW_MS`    | 500              | Maximum time for log lines between telemetry bursts |

### 8.6 Zero command socket

| Value                       | Default             | Description                      |
| --------------------------- | ------------------- | -------------------------------- |
| `CONF_CMD_SOCKET`           | `/tmp/rc-boat.sock` | Unix socket for `cmd.py`         |
| `CONF_CMD_REPLY_TIMEOUT_MS` | 2000                | Maximum time for a Pico answer   |

### 8.7 Zero log

| Value                 | Default            | Description                                   |
| --------------------- | ------------------ | --------------------------------------------- |
| `CONF_LOG_DIR`        | `/var/log/rc-boat` | Log directory. The directory must exist       |
| `CONF_LOG_LORA_QUEUE` | 64                 | Maximum log lines in the LoRa queue           |
| `CONF_LOG_LINE_MAX`   | 512                | Maximum log line length in bytes              |
| `CONF_LOG_PATH_MAX`   | 256                | Maximum log file path length                  |
| `CONF_LOG_FLUSH_MS`   | 1000               | Minimum time between two flushes of the file  |

### 8.8 SWD

| Value                | Default | Description                      |
| -------------------- | ------- | -------------------------------- |
| `CONF_SWD_SWDIO_PIN` | 24      | Zero GPIO for the Pico SWDIO pin |
| `CONF_SWD_SWCLK_PIN` | 25      | Zero GPIO for the Pico SWCLK pin |

## 9. Log file

The Zero program makes a new log file each time it starts. The file name
is the UTC start time, for example `2026-09-24T15-04-05Z.log`. If this file
exists, the program adds a number: `-1`, `-2`, up to `-99`.

Each line has this format:

```
<UTC time> <source> <severity> <text>
```

Data lines (IMU, GPS and CAN) do not have a severity:

```
<UTC time> <source> <text>
```

The severity is `error`, `warning` or `info`. The source is one of these
names: `pico`, `i2c`, `uart`, `imu`, `gps`, `can`, `storage`, `zero`,
`gpio`, `spi`, `lora`, `comm`.

The Zero program also puts each line in the LoRa queue. When the queue is
full, the program removes the oldest line.

## 10. Link protocol (Pico to Zero)

The link uses frames. All values are little-endian.

| Field   | Size (bytes) | Description                                   |
| ------- | ------------ | --------------------------------------------- |
| Marker  | 2            | `AA 55`                                       |
| Length  | 2            | Length of the data field                      |
| Type    | 1            | Message type                                  |
| Counter | 1            | Counter for each type. It increases by 1 for each message |
| Data    | 0 to 65535   | Message data                                  |
| CRC     | 2            | CRC-16/CCITT-FALSE of length, type, counter and data |

If the CRC is not correct, the receiver discards the frame. Then it
searches for the next marker. If the counter of a type increases by more
than 1, the receiver records the lost messages in the log.

### 10.1 Message types

| Type   | Direction    | Name       | Data                                       |
| ------ | ------------ | ---------- | ------------------------------------------ |
| `0x01` | Pico to Zero | `imu0`     | IMU0 sample (84 bytes)                     |
| `0x02` | Pico to Zero | `imu1`     | IMU1 sample (84 bytes)                     |
| `0x03` | Pico to Zero | `imu`      | Combined IMU sample (84 bytes)             |
| `0x04` | Pico to Zero | `gps`      | GPS fix (30 bytes)                         |
| `0x05` | Pico to Zero | `can rx`   | Received CAN frame                         |
| `0x06` | Pico to Zero | `text`     | Severity, source, log text                 |
| `0x07` | Pico to Zero | `reply`    | Request type, request ID (4 bytes), result |
| `0x81` | Zero to Pico | `command`  | Command ID, optional sample count (2 bytes) |
| `0x82` | Zero to Pico | `can tx`   | CAN frame to send                          |
| `0x83` | Zero to Pico | `setting`  | Setting ID, value (4 bytes)                |
| `0x84` | Zero to Pico | `text ack` | Counter of the received `text` message     |

### 10.2 Log lines over the link

The Pico keeps each log line in a queue until the Zero confirms it:

1. The Pico sends the oldest line as a `text` message.
2. The Zero writes the line to the log file. Then it sends a `text ack`
   message with the counter of the line.
3. The Pico removes the line from the queue and sends the next line.

If the Pico does not receive the `text ack` in `CONF_COMM_RESEND_MS`, it
sends the line again.

## 11. LoRa packets

Each LoRa packet has a header of 2 bytes:

| Field   | Size (bytes) | Description                              |
| ------- | ------------ | ---------------------------------------- |
| Type    | 1            | `1` = telemetry, `2` = log line          |
| Counter | 1            | Increases by 1 for each packet           |

A log packet contains one log line after the header. The maximum length of
the text is 253 bytes. The packet does not contain a terminating zero.

### 11.1 Telemetry packet

A telemetry packet is 148 bytes. After the header, it contains three IMU
blocks (`imu0`, `imu1`, combined) and one GPS block. All values are
little-endian. If there is no data for a block, all bytes of the block
are zero.

IMU block (40 bytes):

| Field             | Type       | Unit                                   |
| ----------------- | ---------- | -------------------------------------- |
| Time              | `uint32`   | ms since the Pico start                |
| Acceleration X, Y, Z | 3 × `int16` | 0.01 m/s²                         |
| Gyro X, Y, Z      | 3 × `int16` | 0.001 rad/s                           |
| Magnetic field X, Y, Z | 3 × `int16` | 0.01 µT                         |
| Temperature       | `int16`    | 0.01 °C                                |
| Quaternion W, X, Y, Z | 4 × `int16` | 1/32767                          |
| Roll, pitch, yaw  | 3 × `int16` | 0.0001 rad                            |
| Valid flags       | `uint8`    | Bit 0 accel, 1 gyro, 2 mag, 3 temp, 4 orientation |
| Calibration       | `uint8`    | Bits 7-6 sys, 5-4 accel, 3-2 gyro, 1-0 mag. 0 = none, 3 = full |

The Zero limits each `int16` value to the range of `int16`.

GPS block (26 bytes):

| Field        | Type     | Unit                       |
| ------------ | -------- | -------------------------- |
| Latitude     | `int32`  | 1e-7 degrees               |
| Longitude    | `int32`  | 1e-7 degrees               |
| Height       | `int32`  | mm above the ellipsoid     |
| Ground speed | `uint16` | cm/s                       |
| Heading      | `uint16` | 0.01 degrees               |
| Fix type     | `uint8`  | 0 none, 1 dead reckoning, 2 2D, 3 3D, 4 GNSS and dead reckoning, 5 time only |
| Satellites   | `uint8`  | Number of satellites       |
| Year         | `uint16` | UTC                        |
| Month, day, hour, minute, second | 5 × `uint8` | UTC     |
| Valid flags  | `uint8`  | Bit 0 date, 1 time, 2 fully resolved, 3 magnetic declination |

## 12. Contributing

Only members of SDU Krakens can contribute to this repository.

Before you make changes, read `CONTRIBUTING.md`.

## 13. License

This repository has the MIT license. Refer to the `LICENSE` file.
