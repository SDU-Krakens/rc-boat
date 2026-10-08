#include "calib.h"

#include <float.h>

void calib_open(calib_t *cal) {
  for (int i = 0; i < 3; i++) {
    cal->gyro_bias[i] = 0.0f;
    cal->mag_offset[i] = 0.0f;
    cal->mag_scale[i] = 1.0f;
    cal->acc_sum[i] = 0.0f;
    cal->mag_min[i] = FLT_MAX;
    cal->mag_max[i] = -FLT_MAX;
  }
  cal->n = 0;
}

void calib_gyro_begin(calib_t *cal) {
  for (int i = 0; i < 3; i++) {
    cal->acc_sum[i] = 0.0f;
  }
  cal->n = 0;
}

void calib_gyro_update(calib_t *cal, const float gyro[3]) {
  for (int i = 0; i < 3; i++) {
    cal->acc_sum[i] += gyro[i];
  }
  cal->n++;
}

void calib_gyro_end(calib_t *cal) {
  if (cal->n == 0) {
    return;
  }
  for (int i = 0; i < 3; i++) {
    cal->gyro_bias[i] = cal->acc_sum[i] / (float)cal->n;
  }
}

void calib_mag_begin(calib_t *cal) {
  for (int i = 0; i < 3; i++) {
    cal->mag_min[i] = FLT_MAX;
    cal->mag_max[i] = -FLT_MAX;
  }
}

void calib_mag_update(calib_t *cal, const float mag[3]) {
  for (int i = 0; i < 3; i++) {
    if (mag[i] < cal->mag_min[i]) {
      cal->mag_min[i] = mag[i];
    }
    if (mag[i] > cal->mag_max[i]) {
      cal->mag_max[i] = mag[i];
    }
  }
}

void calib_mag_end(calib_t *cal) {
  float radius[3];
  float avg = 0.0f;

  for (int i = 0; i < 3; i++) {
    if (cal->mag_max[i] <= cal->mag_min[i]) {
      // No usable data collected on this axis, keep the old calibration
      return;
    }
    radius[i] = (cal->mag_max[i] - cal->mag_min[i]) / 2.0f;
    avg += radius[i];
  }
  avg /= 3.0f;

  for (int i = 0; i < 3; i++) {
    // Hard iron: centre of the min/max box
    cal->mag_offset[i] = (cal->mag_max[i] + cal->mag_min[i]) / 2.0f;
    // Soft iron (axis aligned): equalise the three radii
    cal->mag_scale[i] = avg / radius[i];
  }
}

void calib_apply(const calib_t *cal, float gyro[3], float mag[3]) {
  for (int i = 0; i < 3; i++) {
    gyro[i] -= cal->gyro_bias[i];
    mag[i] = (mag[i] - cal->mag_offset[i]) * cal->mag_scale[i];
  }
}
