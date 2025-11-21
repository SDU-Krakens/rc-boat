#include "i2c.h"

int i2c_read(char adr_slave, char adr_register) {
	int result = -1;
	char *string;
	if (0 > asprintf(&string, "i2cget -y 1 %d %d", adr_slave, adr_register))
		return -1; // might need to be < ratehr than >. The same for the
			   // other i2c wrapper
	FILE *pipe;
	char buffer[50];
	char *buff_ret; // buffer return value
	pipe = popen(string, "r");
	free(string);
	if (pipe == NULL) {
		puts("Unable to open process");
		return (-1);
	}
	buff_ret = fgets(buffer, sizeof(buffer), pipe);
	if (buff_ret == NULL) {
		return -1;
	} else {
		sscanf(buffer, "%x", &result);
	}
	pclose(pipe);
	return result;
}
int i2c_write(char adr_slave, char adr_register, char data) {
	char *string;
	if (0 > asprintf(&string, "i2cset -y 1 %d %d %d", adr_slave,
			 adr_register, data))
		return -1;
	FILE *pipe;
	pipe = popen(string, "r");
	free(string);
	if (pipe == NULL) {
		puts("Unable to open process");
		return (-1);
	}
	pclose(pipe);
	return 0;
}
