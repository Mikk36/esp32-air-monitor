#ifndef WIFI_H_
#define WIFI_H_

#define MAX_APs 20

void wifi_task(void *pvParameter);
void initialise_wifi();
void start_wifi();
void stop_wifi();
void wake_wifi();
void sleep_wifi();

#endif
