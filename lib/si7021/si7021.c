#include "si7021.h"
#include "driver/i2c.h"
#include "main.h"

uint16_t read_value(const uint8_t command)
{
    uint8_t result[2];

    i2c_cmd_handle_t cmd;
    cmd = i2c_cmd_link_create();

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (SI7021_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, command, true);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(I2C_NUM_0, cmd, 10 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);

    vTaskDelay(300 / portTICK_PERIOD_MS);

    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (SI7021_ADDR << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, &result[0], I2C_MASTER_ACK);
    i2c_master_read_byte(cmd, &result[1], I2C_MASTER_NACK);
    i2c_master_stop(cmd);

    i2c_master_cmd_begin(I2C_NUM_0, cmd, 10 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);

    uint16_t returnValue = result[0] << 8 | result[1];

    return returnValue;
}

void read_humidity(double *humidity)
{
    uint16_t humiRaw = read_value(RH_READ);
    *humidity = (humiRaw * 125.0 / 65536) - 6;
}

void read_temperature(double *temperature)
{
    uint16_t tempRaw = read_value(POST_RH_TEMP_READ);
    *temperature = (tempRaw * 175.72 / 65536) - 46.85;
}

void read_temp_rh(double *temperature, double *humidity)
{
    read_humidity(humidity);
    read_temperature(temperature);
}

void temperature_humidity_task(void *pvParameter)
{
    struct climateData *data = pvParameter;

    while (1)
    {
        read_temp_rh(&data->temperature, &data->humidity);

        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}