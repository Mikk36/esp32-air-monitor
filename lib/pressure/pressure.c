#include "pressure.h"
#include "../bosch_bmp280/bosch_bmp280.h"
#include "driver/i2c.h"
#include "main.h"

#define I2C_BUFFER_LEN 8
#define BMP280_DATA_INDEX 1
#define ACK_CHECK_EN 0x1
#define ACK_CHECK_DIS 0x0
#define I2C_MASTER_ACK 0
#define I2C_MASTER_NACK 1

struct bmp280_t bmp280;

/*	\Brief: The function is used as I2C bus write
 *	\Return : Status of the I2C write
 *	\param dev_addr : The device address of the sensor
 *	\param reg_addr : Address of the first register, where data is to be written
 *	\param reg_data : It is a value held in the array,
 *		which is written in the register
 *	\param cnt : The no of bytes of data to be written
 */
s8 BMP280_I2C_bus_write(u8 dev_addr, u8 reg_addr, u8 *reg_data, u8 cnt)
{
    s32 iError = BMP280_INIT_VALUE;

    esp_err_t espRc;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev_addr << 1) | I2C_MASTER_WRITE, true);

    i2c_master_write_byte(cmd, reg_addr, true);
    i2c_master_write(cmd, reg_data, cnt, true);
    i2c_master_stop(cmd);

    espRc = i2c_master_cmd_begin(I2C_NUM_0, cmd, 10 / portTICK_PERIOD_MS);
    if (espRc == ESP_OK)
    {
        iError = SUCCESS;
    }
    else
    {
        iError = FAIL;
    }
    i2c_cmd_link_delete(cmd);

    return (s8)iError;
}

/*	\Brief: The function is used as I2C bus read
 *	\Return : Status of the I2C read
 *	\param dev_addr : The device address of the sensor
 *	\param reg_addr : Address of the first register, where data is going to be read
 *	\param reg_data : This is the data read from the sensor, which is held in an array
 *	\param cnt : The no of data to be read
 */
s8 BMP280_I2C_bus_read(u8 dev_addr, u8 reg_addr, u8 *reg_data, u8 cnt)
{
    s32 iError = BMP280_INIT_VALUE;
    esp_err_t espRc;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg_addr, true);

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev_addr << 1) | I2C_MASTER_READ, true);

    if (cnt > 1)
    {
        i2c_master_read(cmd, reg_data, cnt - 1, I2C_MASTER_ACK);
    }
    i2c_master_read_byte(cmd, reg_data + cnt - 1, I2C_MASTER_NACK);
    i2c_master_stop(cmd);

    espRc = i2c_master_cmd_begin(I2C_NUM_0, cmd, 10 / portTICK_PERIOD_MS);

    if (espRc == ESP_OK)
    {
        iError = SUCCESS;
    }
    else
    {
        iError = FAIL;
    }

    i2c_cmd_link_delete(cmd);

    return (s8)iError;
}

void BMP280_delay_msek(u32 msek)
{
    /*Here you can write your own delay routine*/
    // printf("Sleeping for %d ms\n", msek);
    vTaskDelay(msek / portTICK_PERIOD_MS);
}

void pressure_init()
{
    /*--------------------------------------------------------------------------*
 *  By using bmp280 the following structure parameter can be accessed
 *	Bus write function pointer: BMP280_WR_FUNC_PTR
 *	Bus read function pointer: BMP280_RD_FUNC_PTR
 *	Delay function pointer: delay_msec
 *	I2C address: dev_addr
 *--------------------------------------------------------------------------*/
    bmp280.bus_write = BMP280_I2C_bus_write;
    bmp280.bus_read = BMP280_I2C_bus_read;
    bmp280.dev_addr = BMP280_I2C_ADDRESS1;
    bmp280.delay_msec = BMP280_delay_msek;

    bmp280_init(&bmp280);

    bmp280_set_oversamp_pressure(BMP280_ULTRAHIGHRESOLUTION_OVERSAMP_PRESSURE);
    bmp280_set_oversamp_temperature(BMP280_ULTRAHIGHRESOLUTION_OVERSAMP_TEMPERATURE);

    bmp280_set_standby_durn(BMP280_STANDBY_TIME_1_MS);
    bmp280_set_filter(BMP280_FILTER_COEFF_16);
    bmp280_set_power_mode(BMP280_NORMAL_MODE);
}

void pressure_task(void *pvParameter)
{
    struct climateData *data = pvParameter;
    /* The variable used to read real temperature*/
    int32_t temp = BMP280_INIT_VALUE;
    /* The variable used to read real pressure*/
    uint32_t pressure = BMP280_INIT_VALUE;

    pressure_init();

    while (1)
    {
        bmp280_read_pressure_temperature(&pressure, &temp);
        if (pressure != 0 && temp != 0)
        {
            data->pressure = (double)pressure / 100;
        }
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}