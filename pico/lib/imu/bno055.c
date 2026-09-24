#include "bno055.h"
#include "config.h"

#include "pico/time.h"
#include <stdbool.h>
#include <stdio.h>

static int set_mode(bno055_t *bno, uint8_t mode) {
  if (i2c_write(bno->i2c, bno->addr, BNO055_OPR_MODE, mode) != 0) {
    return -1;
  }
  sleep_ms(mode == BNO055_MODE_CONFIG ? BNO055_TO_CONFIG_MS
                                      : BNO055_FROM_CONFIG_MS);
  return 0;
}

static bool probe(bno055_t *bno, uint8_t addr) {
  uint8_t id = 0;
  bno->addr = addr;
  return i2c_read(bno->i2c, addr, BNO055_CHIP_ID, &id, 1) == 0 &&
         id == BNO055_CHIP_ID_VALUE;
}

int bno055_open(bno055_t *bno, i2c_t *i2c, const imu_rot_t *rot) {
  bno->i2c = i2c;
  bno->rot = rot ? *rot : IMU_ROT_IDENTITY;

  if (!probe(bno, BNO055_ADDR_LOW) && !probe(bno, BNO055_ADDR_HIGH)) {
#ifdef CONF_DEBUG
    printf("bno055_open: not found\n");
#endif
    return -1;
  }

  if (i2c_write(bno->i2c, bno->addr, BNO055_SYS_TRIGGER, BNO055_SYS_RST) != 0) {
    return -1;
  }
  sleep_ms(BNO055_RESET_MS);
  if (!probe(bno, bno->addr)) {
    return -1;
  }

  if (set_mode(bno, BNO055_MODE_CONFIG) != 0 ||
      i2c_write(bno->i2c, bno->addr, BNO055_PAGE_ID, 0) != 0 ||
      i2c_write(bno->i2c, bno->addr, BNO055_PWR_MODE, BNO055_PWR_NORMAL) != 0 ||
      i2c_write(bno->i2c, bno->addr, BNO055_UNIT_SEL, BNO055_UNIT_GYRO_RPS) != 0 ||
      set_mode(bno, BNO055_MODE_NDOF) != 0) {
    return -1;
  }

#ifdef CONF_DEBUG
  printf("bno055_open: addr %02x\n", bno->addr);
#endif
  return 0;
}

void bno055_close(bno055_t *bno) { set_mode(bno, BNO055_MODE_CONFIG); }

static int16_t le16(const uint8_t *p) { return (int16_t)((p[1] << 8) | p[0]); }

int bno055_read(bno055_t *bno, imu_sample_t *out) {
  uint8_t buf[BNO055_DATA_LEN];
  uint8_t qbuf[8];
  uint8_t temp, stat;

  out->valid = 0;
  if (i2c_read(bno->i2c, bno->addr, BNO055_ACC_DATA, buf, sizeof(buf)) != 0 ||
      i2c_read(bno->i2c, bno->addr, BNO055_QUA_DATA, qbuf, sizeof(qbuf)) != 0 ||
      i2c_read(bno->i2c, bno->addr, BNO055_TEMP, &temp, 1) != 0 ||
      i2c_read(bno->i2c, bno->addr, BNO055_CALIB_STAT, &stat, 1) != 0) {
    return -1;
  }
  out->timestamp_us = time_us_64();

  for (int i = 0; i < 3; i++) {
    out->accel[i] = le16(&buf[i * 2]) / BNO055_ACC_LSB_PER_MS2;
    out->mag[i] = le16(&buf[6 + i * 2]) / BNO055_MAG_LSB_PER_UT;
    out->gyro[i] = le16(&buf[12 + i * 2]) / BNO055_GYR_LSB_PER_RPS;
  }
  out->temp = (int8_t)temp;

  imu_rotate(&bno->rot, out->accel);
  imu_rotate(&bno->rot, out->gyro);
  imu_rotate(&bno->rot, out->mag);

  // Chip orientation, then into the boat frame: q_boat = q_chip * conj(r)
  float qc[4], r[4];
  for (int i = 0; i < 4; i++) {
    qc[i] = le16(&qbuf[i * 2]) / (float)BNO055_QUA_LSB;
  }
  imu_rot_to_quat(&bno->rot, r);
  r[1] = -r[1];
  r[2] = -r[2];
  r[3] = -r[3];
  imu_quat_mul(qc, r, out->quat);
  imu_quat_to_euler(out->quat, &out->roll, &out->pitch, &out->yaw);

  out->cal.sys = (stat >> BNO055_CAL_SYS_SHIFT) & BNO055_CAL_MASK;
  out->cal.gyro = (stat >> BNO055_CAL_GYR_SHIFT) & BNO055_CAL_MASK;
  out->cal.accel = (stat >> BNO055_CAL_ACC_SHIFT) & BNO055_CAL_MASK;
  out->cal.mag = (stat >> BNO055_CAL_MAG_SHIFT) & BNO055_CAL_MASK;

  out->valid = IMU_VALID_ACCEL | IMU_VALID_GYRO | IMU_VALID_MAG |
               IMU_VALID_TEMP | IMU_VALID_ORIENT;
  return 0;
}

int bno055_cal_get(bno055_t *bno, uint8_t offsets[22]) {
  if (set_mode(bno, BNO055_MODE_CONFIG) != 0) {
    return -1;
  }
  int rc = i2c_read(bno->i2c, bno->addr, BNO055_OFFSETS, offsets,
                    BNO055_OFFSETS_LEN);
  if (set_mode(bno, BNO055_MODE_NDOF) != 0) {
    return -1;
  }
  return rc;
}

int bno055_cal_set(bno055_t *bno, const uint8_t offsets[22]) {
  if (set_mode(bno, BNO055_MODE_CONFIG) != 0) {
    return -1;
  }
  int rc = 0;
  for (int i = 0; i < BNO055_OFFSETS_LEN && rc == 0; i++) {
    rc = i2c_write(bno->i2c, bno->addr, BNO055_OFFSETS + i, offsets[i]);
  }
  if (set_mode(bno, BNO055_MODE_NDOF) != 0) {
    return -1;
  }
  return rc;
}
