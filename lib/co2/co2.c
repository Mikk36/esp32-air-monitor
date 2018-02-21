#include "driver/uart.h"
#include "string.h"
#include "co2.h"
#include "main.h"

static const int RX_BUF_SIZE = 1024;

#define TXD_PIN (17)
#define RXD_PIN (16)

void co2_init()
{
    const uart_config_t uart_config = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE};
    uart_param_config(UART_NUM_1, &uart_config);
    uart_set_pin(UART_NUM_1, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(UART_NUM_1, RX_BUF_SIZE * 2, 0, 0, NULL, 0);
}

int sendData(const char *data, const int len)
{
    const int txBytes = uart_write_bytes(UART_NUM_1, data, len);
    return txBytes;
}

static bool checkCRC(uint8_t *data)
{
    uint8_t crc = 0;
    for (int i = 1; i < 8; i++)
    {
        crc += data[i];
    }
    crc = 255 - crc + 1;

    return crc == data[8];
}

static int readCO2(uint8_t *data)
{
    int responseHigh = (int)data[2];
    int responseLow = (int)data[3];
    int ppm = (256 * responseHigh) + responseLow;
    return ppm;
}

// static int readTemp(uint8_t *data)
// {
//     return data[4] - 40;
// }

// static int readStatus(uint8_t *data)
// {
//     return data[5];
// }

void co2_tx_task()
{
    const int len = 9;
    char cmd[9] = {0xFF, 0x01, 0x86, 0x00, 0x00, 0x00, 0x00, 0x00, 0x79};

    while (1)
    {
        sendData(cmd, len);

        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}

void co2_rx_task(void *pvParameter)
{
    struct climateData *data = pvParameter;
    uint8_t *co2_data = (uint8_t *)malloc(RX_BUF_SIZE + 1);
    while (1)
    {
        const int rxBytes = uart_read_bytes(UART_NUM_1, co2_data, RX_BUF_SIZE, 100 / portTICK_RATE_MS);
        if (rxBytes > 0)
        {
            co2_data[rxBytes] = 0;
            if (rxBytes == 9 && co2_data[1] == 0x86)
            {
                if (!checkCRC(co2_data))
                {
                    printf("CRC error\n");
                    continue;
                }
                int co2 = readCO2(co2_data);
                // int temp = readTemp(data);
                // int status = readStatus(data);

                data->co2 = co2;
            }
        }
    }
    free(co2_data);
}