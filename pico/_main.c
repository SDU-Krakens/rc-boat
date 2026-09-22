#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/uart.h"

// --- Pin Assignments ---
#define BNO_I2C_PORT i2c0
#define BNO_SDA_PIN 4
#define BNO_SCL_PIN 5
#define BNO055_ADDR 0x28

#define GY87_I2C_PORT i2c1
#define GY87_SDA_PIN 2
#define GY87_SCL_PIN 3
#define MPU6050_ADDR 0x68
#define BMP180_ADDR 0x77

#define GPS_UART_ID uart1
#define GPS_BAUD_RATE 9600
#define GPS_TX_PIN 8
#define GPS_RX_PIN 9

// --- BNO055 Registers ---
#define BNO055_OPR_MODE_REG 0x3D
#define BNO055_EUL_DATA_X   0x1A
#define BNO055_MODE_CONFIG  0x00
#define BNO055_MODE_NDOF    0x0C

// --- BMP180 Calibration Parameters ---
int16_t ac1, ac2, ac3, b1, b2, mb, mc, md;
uint16_t ac4, ac5, ac6;

// Telemetry State
typedef struct {
    float latitude;
    float longitude;
    bool gps_valid;
    float heading;
    float roll;
    float pitch;
    float temperature_c;
    float pressure_hpa;
} TelemetryData;

TelemetryData telemetry = {0};

// --- Low-Level I2C Functions ---
void i2c_write_reg(i2c_inst_t *i2c, uint8_t addr, uint8_t reg, uint8_t val) {
    uint8_t buf[2] = {reg, val};
    i2c_write_blocking(i2c, addr, buf, 2, false);
}

void i2c_read_regs(i2c_inst_t *i2c, uint8_t addr, uint8_t reg, uint8_t *buf, uint16_t len) {
    i2c_write_blocking(i2c, addr, &reg, 1, true);
    i2c_read_blocking(i2c, addr, buf, len, false);
}

// --- BMP180 Functions ---
void bmp180_read_calibration() {
    uint8_t calib_buf[22];
    i2c_read_regs(GY87_I2C_PORT, BMP180_ADDR, 0xAA, calib_buf, 22);

    ac1 = (int16_t)((calib_buf[0] << 8) | calib_buf[1]);
    ac2 = (int16_t)((calib_buf[2] << 8) | calib_buf[3]);
    ac3 = (int16_t)((calib_buf[4] << 8) | calib_buf[5]);
    ac4 = (uint16_t)((calib_buf[6] << 8) | calib_buf[7]);
    ac5 = (uint16_t)((calib_buf[8] << 8) | calib_buf[9]);
    ac6 = (uint16_t)((calib_buf[10] << 8) | calib_buf[11]);
    b1  = (int16_t)((calib_buf[12] << 8) | calib_buf[13]);
    b2  = (int16_t)((calib_buf[14] << 8) | calib_buf[15]);
    mb  = (int16_t)((calib_buf[16] << 8) | calib_buf[17]);
    mc  = (int16_t)((calib_buf[18] << 8) | calib_buf[19]);
    md  = (int16_t)((calib_buf[20] << 8) | calib_buf[21]);
}

void bmp180_read_environment(float *temp, float *pressure) {
    // Request Temperature
    i2c_write_reg(GY87_I2C_PORT, BMP180_ADDR, 0xF4, 0x2E);
    sleep_ms(5);
    uint8_t buf[2];
    i2c_read_regs(GY87_I2C_PORT, BMP180_ADDR, 0xF6, buf, 2);
    long ut = (buf[0] << 8) | buf[1];

    // Request Pressure (OSS = 0)
    i2c_write_reg(GY87_I2C_PORT, BMP180_ADDR, 0xF4, 0x34);
    sleep_ms(5);
    i2c_read_regs(GY87_I2C_PORT, BMP180_ADDR, 0xF6, buf, 2);
    long up = (buf[0] << 8) | buf[1];

    // Temperature Calculation
    long x1 = ((ut - ac6) * ac5) >> 15;
    long x2 = (mc << 11) / (x1 + md);
    long b5 = x1 + x2;
    *temp = ((b5 + 8) >> 4) / 10.0f;

    // Pressure Calculation
    long b6 = b5 - 4000;
    x1 = (b2 * (b6 * b6 >> 12)) >> 11;
    x2 = (ac2 * b6) >> 11;
    long x3 = x1 + x2;
    long b3 = (((ac1 * 4 + x3)) + 2) >> 2;
    x1 = (ac3 * b6) >> 13;
    x2 = (b1 * (b6 * b6 >> 12)) >> 16;
    x3 = ((x1 + x2) + 2) >> 2;
    unsigned long b4 = (ac4 * (unsigned long)(x3 + 32768)) >> 15;
    unsigned long b7 = ((unsigned long)up - b3) * 50000;
    long p = (b7 < 0x80000000) ? (b7 * 2) / b4 : (b7 / b4) * 2;
    x1 = (p >> 8) * (p >> 8);
    x1 = (x1 * 3038) >> 16;
    x2 = (-7357 * p) >> 16;
    p = p + ((x1 + x2 + 3791) >> 4);

    *pressure = p / 100.0f; // Convert Pa to hPa
}

// --- GPS Parsing ---
float nmea_to_decimal(const char *nmea_pos, char dir) {
    if (!nmea_pos || strlen(nmea_pos) < 4) return 0.0f;
    float raw = atof(nmea_pos);
    int degrees = (int)(raw / 100);
    float minutes = raw - (degrees * 100);
    float decimal = degrees + (minutes / 60.0f);
    if (dir == 'S' || dir == 'W') decimal = -decimal;
    return decimal;
}

void parse_gprmc_line(char *line) {
    if (strncmp(line, "$GPRMC", 6) != 0 && strncmp(line, "$GNRMC", 6) != 0) return;
    char *tokens[13];
    int token_idx = 0;
    char *p = line;
    tokens[token_idx++] = p;
    while (*p && token_idx < 13) {
        if (*p == ',') {
            *p = '\0';
            tokens[token_idx++] = p + 1;
        }
        p++;
    }
    if (token_idx >= 7 && tokens[2][0] == 'A') {
        telemetry.latitude = nmea_to_decimal(tokens[3], tokens[4][0]);
        telemetry.longitude = nmea_to_decimal(tokens[5], tokens[6][0]);
        telemetry.gps_valid = true;
    } else {
        telemetry.gps_valid = false;
    }
}

void process_gps_rx() {
    static char line_buf[128];
    static int line_idx = 0;
    while (uart_is_readable(GPS_UART_ID)) {
        char c = uart_getc(GPS_UART_ID);
        if (c == '\r' || c == '\n') {
            if (line_idx > 0) {
                line_buf[line_idx] = '\0';
                parse_gprmc_line(line_buf);
                line_idx = 0;
            }
        } else if (line_idx < (int)sizeof(line_buf) - 1) {
            line_buf[line_idx++] = c;
        }
    }
}

int main() {
    stdio_init_all();

    // Init I2C0 (BNO055)
    i2c_init(BNO_I2C_PORT, 100 * 1000);
    gpio_set_function(BNO_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(BNO_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(BNO_SDA_PIN);
    gpio_pull_up(BNO_SCL_PIN);

    // Init I2C1 (GY-87)
    i2c_init(GY87_I2C_PORT, 100 * 1000);
    gpio_set_function(GY87_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(GY87_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(GY87_SDA_PIN);
    gpio_pull_up(GY87_SCL_PIN);

    // Init UART1 (NEO-6M)
    uart_init(GPS_UART_ID, GPS_BAUD_RATE);
    gpio_set_function(GPS_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(GPS_RX_PIN, GPIO_FUNC_UART);

    while (!stdio_usb_connected()) {
        sleep_ms(100);
    }

    // Config BNO055
    i2c_write_reg(BNO_I2C_PORT, BNO055_ADDR, BNO055_OPR_MODE_REG, BNO055_MODE_CONFIG);
    sleep_ms(25);
    i2c_write_reg(BNO_I2C_PORT, BNO055_ADDR, BNO055_OPR_MODE_REG, BNO055_MODE_NDOF);
    sleep_ms(20);

    // Wake MPU-6050 & read BMP180 coefficients
    i2c_write_reg(GY87_I2C_PORT, MPU6050_ADDR, 0x6B, 0x00);
    bmp180_read_calibration();

    uint32_t last_print = 0;

    while (true) {
        process_gps_rx();

        // Read all GY-87 motion registers in background
        uint8_t mpu_buf[14];
        i2c_read_regs(GY87_I2C_PORT, MPU6050_ADDR, 0x3B, mpu_buf, 14);

        uint32_t now = to_ms_since_boot(get_absolute_time());
        if (now - last_print >= 500) {
            last_print = now;

            // Read BNO055 Euler Angles
            uint8_t bno_buf[6];
            i2c_read_regs(BNO_I2C_PORT, BNO055_ADDR, BNO055_EUL_DATA_X, bno_buf, 6);
            telemetry.heading = ((int16_t)(bno_buf[0] | (bno_buf[1] << 8))) / 16.0f;
            telemetry.roll    = ((int16_t)(bno_buf[2] | (bno_buf[3] << 8))) / 16.0f;
            telemetry.pitch   = ((int16_t)(bno_buf[4] | (bno_buf[5] << 8))) / 16.0f;

            // Read GY-87 Temperature & Pressure
            bmp180_read_environment(&telemetry.temperature_c, &telemetry.pressure_hpa);

            // Package output over Serial
            printf("[DATA] ");
            if (telemetry.gps_valid) {
                printf("GPS: %.6f, %.6f | ", telemetry.latitude, telemetry.longitude);
            } else {
                printf("GPS: NO FIX | ");
            }
            printf("BNO055: Head=%6.2f° Roll=%6.2f° Pitch=%6.2f° | ", 
                   telemetry.heading, telemetry.roll, telemetry.pitch);
            printf("ENV: Temp=%.1f°C Press=%.2f hPa\n", 
                   telemetry.temperature_c, telemetry.pressure_hpa);
        }
    }

    return 0;
}
