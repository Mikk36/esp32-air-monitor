#ifndef CO2_H_
#define CO2_H_

void co2_init();
void co2_rx_task(void *pvParameter);
void co2_tx_task();

#endif