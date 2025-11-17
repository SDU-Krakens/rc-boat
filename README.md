# RC Boat

RC Boat firmware for the RC Boat prototype.

## Contributing

Only SDU Kraken members may contribute to this repository.
</br>
Please read `CONTRIBUTING.md` before making any changes.

## License

Licensed under the MIT license.
=======
# README file for the MPU 

## How to use the functions

### int i2c_write(char adr_slave, char adr_register, char data);

adr_slave - Adress of the slve device. Normally 8 bit hex value.
adr_register - Adress (on the slave device) of the register you want to write into. Normally 8 bit hex value.
data - The value you want to write into the register. Normally 8 bit.
returns -> -1 if an error occured, otherwise 0

### char i2c_read(char adr_slave, char adr_register);

adr_slave - Adress of the slve device. Normally 8 bit hex value.
adr_register - Adress (on the slave device) of the register you want to read from. Normally 8 bit hex value.
returns -> The value at the provided adress register. -1 if an error occured.

### void get_raw_acc(float \*xx, float \*yy, float \*zz);

xx / yy / zz - adress of a float where the acceleration values are supposed to be written. Not sure of the unit yet.
