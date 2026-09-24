#pragma once

typedef struct {
  float q[4]; // w, x, y, z
  float beta;
} madgwick_t;

void madgwick_open(madgwick_t *m, float beta);
void madgwick_update(madgwick_t *m, const float gyro[3], const float accel[3],
                     const float mag[3], float dt);
void madgwick_euler(const madgwick_t *m, float *roll, float *pitch, float *yaw);
