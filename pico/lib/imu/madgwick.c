// Madgwick MARG orientation filter (gyro + accel + mag), after
// S. Madgwick, "An efficient orientation filter for inertial and
// inertial/magnetic sensor arrays", 2010.
#include "madgwick.h"
#include "imu_types.h"

#include <math.h>

void madgwick_open(madgwick_t *m, float beta) {
  m->q[0] = 1.0f;
  m->q[1] = 0.0f;
  m->q[2] = 0.0f;
  m->q[3] = 0.0f;
  m->beta = beta;
}

static float inv_norm3(float x, float y, float z) {
  float n = sqrtf(x * x + y * y + z * z);
  return n > 0.0f ? 1.0f / n : 0.0f;
}

void madgwick_update(madgwick_t *m, const float gyro[3], const float accel[3],
                     const float mag[3], float dt) {
  float q0 = m->q[0], q1 = m->q[1], q2 = m->q[2], q3 = m->q[3];
  float gx = gyro[0], gy = gyro[1], gz = gyro[2];

  // Rate of change of quaternion from gyroscope
  float qdot0 = 0.5f * (-q1 * gx - q2 * gy - q3 * gz);
  float qdot1 = 0.5f * (q0 * gx + q2 * gz - q3 * gy);
  float qdot2 = 0.5f * (q0 * gy - q1 * gz + q3 * gx);
  float qdot3 = 0.5f * (q0 * gz + q1 * gy - q2 * gx);

  float an = inv_norm3(accel[0], accel[1], accel[2]);
  float mn = inv_norm3(mag[0], mag[1], mag[2]);

  if (an > 0.0f && mn > 0.0f) {
    float ax = accel[0] * an, ay = accel[1] * an, az = accel[2] * an;
    float mx = mag[0] * mn, my = mag[1] * mn, mz = mag[2] * mn;

    // Auxiliary variables to avoid repeated arithmetic
    float _2q0mx = 2.0f * q0 * mx;
    float _2q0my = 2.0f * q0 * my;
    float _2q0mz = 2.0f * q0 * mz;
    float _2q1mx = 2.0f * q1 * mx;
    float _2q0 = 2.0f * q0;
    float _2q1 = 2.0f * q1;
    float _2q2 = 2.0f * q2;
    float _2q3 = 2.0f * q3;
    float _2q0q2 = 2.0f * q0 * q2;
    float _2q2q3 = 2.0f * q2 * q3;
    float q0q0 = q0 * q0;
    float q0q1 = q0 * q1;
    float q0q2 = q0 * q2;
    float q0q3 = q0 * q3;
    float q1q1 = q1 * q1;
    float q1q2 = q1 * q2;
    float q1q3 = q1 * q3;
    float q2q2 = q2 * q2;
    float q2q3 = q2 * q3;
    float q3q3 = q3 * q3;

    // Reference direction of Earth's magnetic field (distortion compensation)
    float hx = mx * q0q0 - _2q0my * q3 + _2q0mz * q2 + mx * q1q1 +
               _2q1 * my * q2 + _2q1 * mz * q3 - mx * q2q2 - mx * q3q3;
    float hy = _2q0mx * q3 + my * q0q0 - _2q0mz * q1 + _2q1mx * q2 -
               my * q1q1 + my * q2q2 + _2q2 * mz * q3 - my * q3q3;
    float _2bx = sqrtf(hx * hx + hy * hy);
    float _2bz = -_2q0mx * q2 + _2q0my * q1 + mz * q0q0 + _2q1mx * q3 -
                 mz * q1q1 + _2q2 * my * q3 - mz * q2q2 + mz * q3q3;
    float _4bx = 2.0f * _2bx;
    float _4bz = 2.0f * _2bz;

    // Gradient descent corrective step
    float s0 = -_2q2 * (2.0f * q1q3 - _2q0q2 - ax) +
               _2q1 * (2.0f * q0q1 + _2q2q3 - ay) -
               _2bz * q2 * (_2bx * (0.5f - q2q2 - q3q3) + _2bz * (q1q3 - q0q2) - mx) +
               (-_2bx * q3 + _2bz * q1) * (_2bx * (q1q2 - q0q3) + _2bz * (q0q1 + q2q3) - my) +
               _2bx * q2 * (_2bx * (q0q2 + q1q3) + _2bz * (0.5f - q1q1 - q2q2) - mz);
    float s1 = _2q3 * (2.0f * q1q3 - _2q0q2 - ax) +
               _2q0 * (2.0f * q0q1 + _2q2q3 - ay) -
               4.0f * q1 * (1.0f - 2.0f * q1q1 - 2.0f * q2q2 - az) +
               _2bz * q3 * (_2bx * (0.5f - q2q2 - q3q3) + _2bz * (q1q3 - q0q2) - mx) +
               (_2bx * q2 + _2bz * q0) * (_2bx * (q1q2 - q0q3) + _2bz * (q0q1 + q2q3) - my) +
               (_2bx * q3 - _4bz * q1) * (_2bx * (q0q2 + q1q3) + _2bz * (0.5f - q1q1 - q2q2) - mz);
    float s2 = -_2q0 * (2.0f * q1q3 - _2q0q2 - ax) +
               _2q3 * (2.0f * q0q1 + _2q2q3 - ay) -
               4.0f * q2 * (1.0f - 2.0f * q1q1 - 2.0f * q2q2 - az) +
               (-_4bx * q2 - _2bz * q0) * (_2bx * (0.5f - q2q2 - q3q3) + _2bz * (q1q3 - q0q2) - mx) +
               (_2bx * q1 + _2bz * q3) * (_2bx * (q1q2 - q0q3) + _2bz * (q0q1 + q2q3) - my) +
               (_2bx * q0 - _4bz * q2) * (_2bx * (q0q2 + q1q3) + _2bz * (0.5f - q1q1 - q2q2) - mz);
    float s3 = _2q1 * (2.0f * q1q3 - _2q0q2 - ax) +
               _2q2 * (2.0f * q0q1 + _2q2q3 - ay) +
               (-_4bx * q3 + _2bz * q1) * (_2bx * (0.5f - q2q2 - q3q3) + _2bz * (q1q3 - q0q2) - mx) +
               (-_2bx * q0 + _2bz * q2) * (_2bx * (q1q2 - q0q3) + _2bz * (q0q1 + q2q3) - my) +
               _2bx * q1 * (_2bx * (q0q2 + q1q3) + _2bz * (0.5f - q1q1 - q2q2) - mz);

    float sn = sqrtf(s0 * s0 + s1 * s1 + s2 * s2 + s3 * s3);
    if (sn > 0.0f) {
      sn = 1.0f / sn;
      qdot0 -= m->beta * s0 * sn;
      qdot1 -= m->beta * s1 * sn;
      qdot2 -= m->beta * s2 * sn;
      qdot3 -= m->beta * s3 * sn;
    }
  }

  // Integrate rate of change of quaternion
  q0 += qdot0 * dt;
  q1 += qdot1 * dt;
  q2 += qdot2 * dt;
  q3 += qdot3 * dt;

  float qn = sqrtf(q0 * q0 + q1 * q1 + q2 * q2 + q3 * q3);
  if (qn > 0.0f) {
    qn = 1.0f / qn;
    m->q[0] = q0 * qn;
    m->q[1] = q1 * qn;
    m->q[2] = q2 * qn;
    m->q[3] = q3 * qn;
  }
}

void madgwick_euler(const madgwick_t *m, float *roll, float *pitch,
                    float *yaw) {
  imu_quat_to_euler(m->q, roll, pitch, yaw);
}
