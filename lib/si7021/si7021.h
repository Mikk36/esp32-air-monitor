#ifndef SI7021_H_
#define SI7021_H_

#define SI7021_ADDR 0x40

#define RH_READ 0xE5
#define TEMP_READ 0xE3
#define POST_RH_TEMP_READ 0xE0
#define RESET 0xFE
#define USER1_READ 0xE7
#define USER1_WRITE 0xE6

// #define I2C_MASTER_ACK 0
// #define I2C_MASTER_NACK 1

void temperature_humidity_task(void *pvParameter);
void read_temp_rh(double *temperature, double *humidity);

#endif
