#include "i2c.h"
#include "mpu6050.h"
#include "unity.h"

void setUp(void) {}

void tearDown(void) {}

int main() {
  int test = i2c_read(adr_mpu, 0x26);
  imu_config();
  i2c_write(adr_mpu, 0x26, 0xaa);
  printf("\n%d", i2c_read(adr_mpu, 0x26));
  printf("\n\n");
  printf("test: %d\n", test);

  int x1, y1, z1;
  get_raw_acc(&x1, &y1, &z1);
  printf("\nraw acceleration data: %d, %d, %d", x1, y1, z1);
  float x2, y2, z2;
  get_unfil_acc(&x2, &y2, &z2);
  printf("\nunfiltered acceleration data: %f, %f, %f", x2, y2, z2);
  return 0;
}
