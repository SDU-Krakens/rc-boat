
/*
 * date: 2025-11-21
 * author: ps
 * title: library for the use of i2c on raspberry pi
 * dependencies:
 *	- needs i2c-tools package (version? 1.69~deb13u1 arm64)
 */
#pragma once

#include <stdio.h>
#include <stdlib.h>

// functions declarations vv

int i2c_read(char adr_slave, char adr_register);
int i2c_write(char adr_slave, char adr_register, char data);
