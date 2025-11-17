# this is a pseud code made to write down the i2c workflow without having access to the RPi

## global definitions
	#define adr 0xFF // to be defined later
	#define sampleRate 10 // [kHz] // to be defined later
	#define GYRO_RANGE 0 //Select which gyroscope range to use (see the table below) - Default is 0
	//	Gyroscope Range
	//	0	+/- 250 degrees/second
	//	1	+/- 500 degrees/second
	//	2	+/- 1000 degrees/second
	//	3	+/- 2000 degrees/second
	#define ACCEL_RANGE 0 //Select which accelerometer range to use (see the table below) - Default is 0
	//	Accelerometer Range
	//	0	+/- 2g
	//	1	+/- 4g
	//	2	+/- 8g
	//	3	+/- 16g
	//See the MPU6000 Register Map for more information

## functions declarations

	void getRawAcc(float *xx, float *yy, float *zz){
		uint16t x = (i2cRead(adr, 0x3B) << 8); // read high byte
		x |= i2cRead(adr, 0x3C); // read low byte and add to high one
		uint16t y = (i2cRead(adr, 0x3D) << 8);
		y |= i2cRead(adr, 0x3E);
		uint16t z = (i2cRead(adr, 0x3F) << 8);
		z |= i2cRead(adr, 0x40);
		*xx = (float)x; // change into float and return
		*yy = (float)y;
		*zz = (float)z;
	}
	void getRawGyro(float *roll, float *pitch, float *yaw){
		uint16t x = (i2cRead(adr, 0x43) << 8); // read high byte
		x |= i2cRead(adr, 0x44); // read low byte and add to high one
		uint16t y = (i2cRead(adr, 0x45) << 8);
		y |= i2cRead(adr, 0x46);
		uint16t z = (i2cRead(adr, 0x47) << 8);
		z |= i2cRead(adr, 0x48);
		*roll = (float)x; // change into float and return
		*pitch = (float)y;
		*yaw = (float)z;
	}
	void imuConfig(){
		uint8t pwr_mgmt_1 = i2cRead(adr, 0x6B); // read the power management byte
		i2cWrite(adr, 0x6B, (pwr_mgmt_1&(~(1<<6)))); // write back the byte with sleep disabled
		// i2cWrite(adr, 0x6B, 0x00); // turn off sleep mode
		uint8t gyroOutRate;
		if (DLPF disabled){ // decide what the GOR is 
			gyroOutRate = 8;
		} else{
			gyroOutRate = 1;
		}
		uint8t sampleDiv = gyroOutRate/sampleRate-1; // calculate SD
		i2cWrite(adr, 0x19, sampleDiv); // send SD to MPU
		i2cWrite(adr, 0x1B, GYRO_RANGE<<3); // sets up the range of gyroscope
		i2cWrite(adr, 0x1C, ACCEL_RANGE<<3);
	}
	
	void selfTest(){
		str = out_without_TS - out_with_TS
	}

## i2c controll code (idk if works)

	f_dev = open("/dev/i2c-1", O_RDWR); // Open the I2C device file
	if (f_dev < 0) {                    // Catch errors
	std::cout
		<< "ERR (MPU6050.cpp:MPU6050()): Failed to open /dev/i2c-1. Please "
		"check that I2C is enabled with raspi-config\n"; // Print error
								// message
	}

	status = ioctl(f_dev, I2C_SLAVE,
			MPU6050_addr); // Set the I2C bus to use the correct address
	if (status < 0) {
	std::cout
		<< "ERR (MPU6050.cpp:MPU6050()): Could not get I2C bus with " << addr
		<< " address. Please confirm that this address is correct\n"; // Print
									// error
									// message
	}

