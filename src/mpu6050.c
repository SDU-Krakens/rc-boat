// Wrapper for i2c communication with IMU 
// dependencies: 
//    - needs i2c-tools package (version? 1.69~deb13u1 arm64)
// date: 2025-11-11
// author: PS
//
#include "mpu6050.h"

char i2c_read(char adr_slave, char adr_register);
void getRawAcc(float *xx, float *yy, float *zz);
void imuConfig();
int i2c_write(char adr_slave, char adr_register, char data);

int main() {
  int test = i2c_read(adr_mpu, 0x26);
  imuConfig();
  printf("test: %d\n", test);
  float x, y, z;
  getRawAcc(&x, &y, &z);
  printf("\n%f, %f, %f", x, y, z);
  i2c_write(adr_mpu, 0x26, 0xaa);
  printf("\n%d", i2c_read(adr_mpu, 0x26));
  printf("\n\n");
  return 0;
}

char i2c_read(char adr_slave, char adr_register){
    char result = -1;
    char* string;
    if(0 > asprintf(&string, "i2cget -y 1 %d %d", adr_slave, adr_register)) return -1; // might need to be < ratehr than >. The same for the other i2c wrapper
    FILE* pipe;
    char buffer[50];
    char *buff_ret; // buffer return value
    pipe = popen(string, "r");
    free(string);
    if( pipe == NULL){
        puts("Unable to open process");
        return(-1);
    }
    buff_ret = fgets(buffer, sizeof(buffer), pipe);
    if (buff_ret == NULL) {
        return -1;
    }else{
         sscanf(buffer, "%x", &result);
    }
    pclose(pipe);
    return result;
}
int i2c_write(char adr_slave, char adr_register, char data){
    char* string;
    if(0 > asprintf(&string, "i2cset -y 1 %d %d %d", adr_slave, adr_register, data)) return -1;
    FILE* pipe;
    pipe = popen(string, "r");
    free(string);
    if( pipe == NULL){
        puts("Unable to open process");
        return(-1);
    }
    pclose(pipe);
    return 0;
}
void getRawAcc(int *xx, int *yy, int *zz){
    u_int16_t x = (i2c_read(adr_mpu, 0x3b) << 8); // read high byte
    x |= i2c_read(adr_mpu, 0x3C); // read low byte and add to high one
    u_int16_t y = (i2c_read(adr_mpu, 0x3D) << 8);
    y |= i2c_read(adr_mpu, 0x3E);
    u_int16_t z = (i2c_read(adr_mpu, 0x3F) << 8);
    z |= i2c_read(adr_mpu, 0x40);
    *xx = (float)x; // change into float and return
    *yy = (float)y;
    *zz = (float)z;
}
void imuConfig(){
//    uint8t pwr_mgmt_1 = i2cread(adr, 0x6B); // read the power management byte
//    i2cwrite(adr_mpu, 0x6b, (pwr_mgmt_1&(~(1<<6)))); // write back the byte with sleep disabled
   i2c_write(adr_mpu, 0x6b, 0x00); // turn off sleep mode
   u_int8_t gyro_out_rate;
   if (dlpf == 0 || dlpf == 7){ // decide what the GOR is 
     gyro_out_rate = 8;
   } else{
     gyro_out_rate = 1;
   }
   u_int8_t samplediv = gyro_out_rate/sample_rate-1; // calculate SD
   i2c_write(adr_mpu, 0x19, samplediv); // send SD to MPU
   u_int8_t config_val = 0x00 | dlpf | (ext_synq<<3);
   i2c_write(adr_mpu, 0x1a, config_val); // external synq and dlpf
   i2c_write(adr_mpu, 0x1b, GYRO_RANGE<<3); // sets up the range of gyroscope
   i2c_write(adr_mpu, 0x1c, ACCEL_RANGE<<3);
}
// void getRawGyro(float *roll, float *pitch, float *yaw){
//   uint16t x = (i2cRead(adr, 0x43) << 8); // read high byte
//   x |= i2cRead(adr, 0x44); // read low byte and add to high one
//   uint16t y = (i2cRead(adr, 0x45) << 8);
//   y |= i2cRead(adr, 0x46);
//   uint16t z = (i2cRead(adr, 0x47) << 8);
//   z |= i2cRead(adr, 0x48);
//   *roll = (float)x; // change into float and return
//   *pitch = (float)y;
//   *yaw = (float)z;
// }
// 
// void selfTest(){
//   str = out_without_TS - out_with_TS
// }
