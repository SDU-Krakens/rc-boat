#include "icm20948.h"
#include "config.h"

#include "pico/time.h"
#include <math.h>
#include <stdio.h>

#define G_TO_MS2 9.80665f
#define DEG_TO_RAD ((float)M_PI / 180.0f)
#define RAW_FULL_SCALE 32768.0f

static int select_bank(icm20948_t *icm, uint8_t bank) {
  return i2c_write(icm->i2c, icm->addr, ICM20948_REG_BANK_SEL,
                   bank << ICM20948_BANK_SHIFT);
}

static int write_reg(icm20948_t *icm, uint8_t bank, uint8_t reg, uint8_t val) {
  if (select_bank(icm, bank) != 0) {
    return -1;
  }
  return i2c_write(icm->i2c, icm->addr, reg, val);
}

static int read_reg(icm20948_t *icm, uint8_t bank, uint8_t reg, uint8_t *val) {
  if (select_bank(icm, bank) != 0) {
    return -1;
  }
  return i2c_read(icm->i2c, icm->addr, reg, val, 1);
}

// Single register access to the AK09916 through I2C master slave 4
static int slv4_transfer(icm20948_t *icm, bool read, uint8_t reg, uint8_t *val) {
  uint8_t addr = AK09916_ADDR | (read ? ICM20948_SLV_READ : 0);
  if (write_reg(icm, 3, ICM20948_I2C_SLV4_ADDR, addr) != 0 ||
      write_reg(icm, 3, ICM20948_I2C_SLV4_REG, reg) != 0) {
    return -1;
  }
  if (!read && write_reg(icm, 3, ICM20948_I2C_SLV4_DO, *val) != 0) {
    return -1;
  }
  if (write_reg(icm, 3, ICM20948_I2C_SLV4_CTRL,
                ICM20948_SLV_EN | ICM20948_MAG_READ_DLY) != 0) {
    return -1;
  }

  absolute_time_t deadline = make_timeout_time_us(ICM20948_SLV4_TIMEOUT_US);
  uint8_t status = 0;
  do {
    if (read_reg(icm, 0, ICM20948_I2C_MST_STATUS, &status) != 0) {
      return -1;
    }
    if (time_reached(deadline)) {
      return -1;
    }
  } while (!(status & ICM20948_SLV4_DONE));

  if (read) {
    return read_reg(icm, 3, ICM20948_I2C_SLV4_DI, val);
  }
  return 0;
}

static int accel_fs(uint32_t range_g, float *sens) {
  switch (range_g) {
  case 2: *sens = 16384.0f; return 0;
  case 4: *sens = 8192.0f; return 1;
  case 8: *sens = 4096.0f; return 2;
  case 16: *sens = 2048.0f; return 3;
  default: return -1;
  }
}

static int gyro_fs(uint32_t range_dps, float *sens) {
  switch (range_dps) {
  case 250: *sens = 131.0f; return 0;
  case 500: *sens = 65.5f; return 1;
  case 1000: *sens = 32.8f; return 2;
  case 2000: *sens = 16.4f; return 3;
  default: return -1;
  }
}

static int setup_sensors(icm20948_t *icm) {
  float accel_sens, gyro_sens;
  int afs = accel_fs(CONF_ICM_ACCEL_RANGE_G, &accel_sens);
  int gfs = gyro_fs(CONF_ICM_GYRO_RANGE_DPS, &gyro_sens);
  if (afs < 0 || gfs < 0 || CONF_ICM_DLPF > ICM20948_DLPF_MAX) {
    return -1;
  }
  icm->accel_scale = G_TO_MS2 / accel_sens;
  icm->gyro_scale = DEG_TO_RAD / gyro_sens;

  uint8_t gyro_div =
      (uint8_t)(ICM20948_GYRO_BASE_HZ / CONF_IMU_SAMPLE_HZ + 0.5f) - 1;
  uint16_t accel_div =
      (uint16_t)(ICM20948_ACCEL_BASE_HZ / CONF_IMU_SAMPLE_HZ + 0.5f) - 1;
  uint8_t dlpf = CONF_ICM_DLPF << ICM20948_DLPF_SHIFT;

  if (write_reg(icm, 2, ICM20948_GYRO_SMPLRT_DIV, gyro_div) != 0 ||
      write_reg(icm, 2, ICM20948_GYRO_CONFIG_1,
                dlpf | (gfs << ICM20948_FS_SHIFT) | ICM20948_FCHOICE) != 0 ||
      write_reg(icm, 2, ICM20948_ACCEL_SMPLRT_DIV_1, accel_div >> 8) != 0 ||
      write_reg(icm, 2, ICM20948_ACCEL_SMPLRT_DIV_2, accel_div & 0xFF) != 0 ||
      write_reg(icm, 2, ICM20948_ACCEL_CONFIG,
                dlpf | (afs << ICM20948_FS_SHIFT) | ICM20948_FCHOICE) != 0) {
    return -1;
  }
  return 0;
}

static int setup_mag(icm20948_t *icm) {
  if (write_reg(icm, 0, ICM20948_USER_CTRL, ICM20948_USER_I2C_MST_RST) != 0 ||
      write_reg(icm, 0, ICM20948_USER_CTRL, ICM20948_USER_I2C_MST_EN) != 0 ||
      write_reg(icm, 3, ICM20948_I2C_MST_CTRL,
                ICM20948_MST_CLK_345KHZ | ICM20948_MST_P_NSR) != 0) {
    return -1;
  }

  uint8_t wia = 0;
  if (slv4_transfer(icm, true, AK09916_WIA2, &wia) != 0 ||
      wia != AK09916_WIA2_VALUE) {
#ifdef CONF_DEBUG
    printf("icm20948: AK09916 not found (%02x)\n", wia);
#endif
    return -1;
  }

  uint8_t val = AK09916_SOFT_RESET;
  if (slv4_transfer(icm, false, AK09916_CNTL3, &val) != 0) {
    return -1;
  }
  sleep_ms(ICM20948_MAG_RESET_MS);

  val = AK09916_MODE_CONT_100HZ;
  if (slv4_transfer(icm, false, AK09916_CNTL2, &val) != 0) {
    return -1;
  }

  // Slave 0 continuously copies ST1..ST2 into EXT_SLV_SENS_DATA
  if (write_reg(icm, 3, ICM20948_I2C_SLV0_ADDR,
                AK09916_ADDR | ICM20948_SLV_READ) != 0 ||
      write_reg(icm, 3, ICM20948_I2C_SLV0_REG, AK09916_ST1) != 0 ||
      write_reg(icm, 3, ICM20948_I2C_MST_DELAY_CTRL,
                ICM20948_SLV0_DELAY_EN) != 0 ||
      write_reg(icm, 3, ICM20948_I2C_SLV0_CTRL,
                ICM20948_SLV_EN | AK09916_READ_LEN) != 0) {
    return -1;
  }
  return 0;
}

int icm20948_open(icm20948_t *icm, i2c_t *i2c, const imu_rot_t *rot) {
  const uint8_t addrs[] = {ICM20948_ADDR_LOW, ICM20948_ADDR_HIGH};
  uint8_t who = 0;

  icm->i2c = i2c;
  icm->rot = rot ? *rot : IMU_ROT_IDENTITY;
  icm->last_us = 0;
  calib_open(&icm->cal);
  madgwick_open(&icm->filter, CONF_MADGWICK_BETA);

  icm->addr = 0;
  for (size_t i = 0; i < sizeof(addrs); i++) {
    icm->addr = addrs[i];
    if (read_reg(icm, 0, ICM20948_WHO_AM_I, &who) == 0 &&
        who == ICM20948_WHO_AM_I_VALUE) {
      break;
    }
    icm->addr = 0;
  }
  if (!icm->addr) {
#ifdef CONF_DEBUG
    printf("icm20948_open: not found\n");
#endif
    return -1;
  }

  if (write_reg(icm, 0, ICM20948_PWR_MGMT_1, ICM20948_PWR_RESET) != 0) {
    return -1;
  }
  sleep_ms(ICM20948_RESET_MS);
  if (write_reg(icm, 0, ICM20948_PWR_MGMT_1, ICM20948_PWR_CLK_AUTO) != 0 ||
      write_reg(icm, 0, ICM20948_PWR_MGMT_2, ICM20948_PWR_ALL_ON) != 0) {
    return -1;
  }
  sleep_ms(ICM20948_WAKE_MS);

  if (setup_sensors(icm) != 0 || setup_mag(icm) != 0) {
    return -1;
  }

  // Reads use bank 0 only
  if (select_bank(icm, 0) != 0) {
    return -1;
  }

#ifdef CONF_DEBUG
  printf("icm20948_open: addr %02x\n", icm->addr);
#endif
  return 0;
}

void icm20948_close(icm20948_t *icm) {
  write_reg(icm, 0, ICM20948_PWR_MGMT_1, ICM20948_PWR_RESET);
}

static int16_t be16(const uint8_t *p) { return (int16_t)((p[0] << 8) | p[1]); }
static int16_t le16(const uint8_t *p) { return (int16_t)((p[1] << 8) | p[0]); }

int icm20948_read_raw(icm20948_t *icm, float accel[3], float gyro[3],
                      float mag[3], float *temp, bool *mag_valid) {
  uint8_t buf[ICM20948_BURST_LEN];
  if (i2c_read(icm->i2c, icm->addr, ICM20948_ACCEL_XOUT_H, buf, sizeof(buf)) !=
      0) {
    return -1;
  }

  for (int i = 0; i < 3; i++) {
    accel[i] = be16(&buf[i * 2]) * icm->accel_scale;
    gyro[i] = be16(&buf[6 + i * 2]) * icm->gyro_scale;
  }
  *temp = be16(&buf[12]) / ICM20948_TEMP_SENS + ICM20948_TEMP_OFFSET;

  // AK09916 axes: X same as accel, Y and Z inverted
  const uint8_t *m = &buf[14];
  uint8_t st1 = m[0], st2 = m[AK09916_READ_LEN - 1];
  mag[0] = le16(&m[1]) * AK09916_UT_PER_LSB;
  mag[1] = -le16(&m[3]) * AK09916_UT_PER_LSB;
  mag[2] = -le16(&m[5]) * AK09916_UT_PER_LSB;
  *mag_valid = (st1 & AK09916_ST1_DRDY) && !(st2 & AK09916_ST2_HOFL);

  return 0;
}

static uint8_t cal_level(bool done) { return done ? IMU_CAL_FULL : IMU_CAL_NONE; }

int icm20948_read(icm20948_t *icm, imu_sample_t *out) {
  bool mag_valid;
  if (icm20948_read_raw(icm, out->accel, out->gyro, out->mag, &out->temp,
                        &mag_valid) != 0) {
    out->valid = 0;
    return -1;
  }

  uint64_t now = time_us_64();
  float dt = icm->last_us ? (now - icm->last_us) / 1e6f
                          : 1.0f / CONF_IMU_SAMPLE_HZ;
  icm->last_us = now;
  out->timestamp_us = now;

  calib_apply(&icm->cal, out->gyro, out->mag);
  imu_rotate(&icm->rot, out->accel);
  imu_rotate(&icm->rot, out->gyro);
  imu_rotate(&icm->rot, out->mag);

  out->valid = IMU_VALID_ACCEL | IMU_VALID_GYRO | IMU_VALID_TEMP;
  if (mag_valid) {
    out->valid |= IMU_VALID_MAG;
    madgwick_update(&icm->filter, out->gyro, out->accel, out->mag, dt);
    out->valid |= IMU_VALID_ORIENT;
  }

  for (int i = 0; i < 4; i++) {
    out->quat[i] = icm->filter.q[i];
  }
  madgwick_euler(&icm->filter, &out->roll, &out->pitch, &out->yaw);

  bool gyro_done = icm->cal.n > 0;
  bool mag_done = icm->cal.mag_max[0] > icm->cal.mag_min[0];
  out->cal.gyro = cal_level(gyro_done);
  out->cal.mag = cal_level(mag_done);
  out->cal.accel = IMU_CAL_NONE;
  out->cal.sys = cal_level(gyro_done && mag_done);

  return 0;
}
