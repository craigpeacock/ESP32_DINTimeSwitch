#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "esp_log.h"
#include "esp_event.h"
#include "driver/gpio.h"

#include "defines.h"
#include "wifi.h"
#include "ntp.h"
#include "http.h"
#include "gpio.h"
#include "aemo.h"

static const char *TAG = "main";

struct aemo aemo_history[HISTORY];
uint8_t aemo_idx = 0;

void parse_time(const struct tm *timeinfo);

void app_main(void)
{
	time_t now;
	struct tm timeinfo;
	uint8_t state = FETCH;
	uint8_t number_tries = 0;
	int8_t previous_period = -1;

	ESP_LOGW(TAG, "ESP32 EVSE Smart Time Switch V%s", VERSION);

	// Set timezone to Adelaide Time
	setenv("TZ", "ACST-9:30ACDT,M10.1.0,M4.1.0/3", 1);
	tzset();

	// Initialise GPIO
	gpio_init();
	// Initialise network
	wifi_init_sta();
	// Initialise network time
	ntp_init();
	display_local_time();
	// Initialise http server
	http_server_init();

	aemo_data.region = "SA1";

	while (1) {

		// Get current time
		time(&now);
		// Populate timeinfo structure
		localtime_r(&now, &timeinfo);

		switch (state) {

			case IDLE:
				/* 20 seconds after a 5 minute period, start fetching a new JSON file */
				if ((!(timeinfo.tm_min % 5)) & (timeinfo.tm_sec == 20)) {
					state = FETCH;
					number_tries = 0;
					ESP_LOGI(TAG, "Fetching data for next settlement period");
				}
				break;

			case FETCH:
				/* Start fetching a new JSON file. We keep trying every 5 seconds until */
				/* the settlement time is different from the previous period */

				aemo_get_price(&aemo_data);

				number_tries++;

				if (aemo_data.settlement.tm_min != previous_period) {
					/* Change in settlement time, log new period */
					previous_period = aemo_data.settlement.tm_min;

					ESP_LOGI(TAG, "Current settlement period %04d-%02d-%02d %02d:%02d:%02d",
						aemo_data.settlement.tm_year + 1900,
						aemo_data.settlement.tm_mon + 1,
						aemo_data.settlement.tm_mday,
						aemo_data.settlement.tm_hour,
						aemo_data.settlement.tm_min,
						aemo_data.settlement.tm_sec);
					ESP_LOGI(TAG, "Price: $%.02f MWh",aemo_data.price);
					ESP_LOGI(TAG, "Total Demand: %.02f MW",aemo_data.totaldemand);
					ESP_LOGI(TAG, "Export: %.02f MW",aemo_data.netinterchange);
					ESP_LOGI(TAG, "Scheduled Generation (Baseload): %.02f MW",aemo_data.scheduledgeneration);
					ESP_LOGI(TAG, "Semi Scheduled Generation (Renewable): %.02f MW",aemo_data.semischeduledgeneration);
					ESP_LOGI(TAG, "Renewables: %.01f %%",aemo_data.renewables);

					aemo_history[aemo_idx].settlement = aemo_data.settlement;
					aemo_history[aemo_idx].price = aemo_data.price;
					aemo_history[aemo_idx].totaldemand = aemo_data.totaldemand;
					aemo_history[aemo_idx].netinterchange = aemo_data.netinterchange;
					aemo_history[aemo_idx].scheduledgeneration = aemo_data.scheduledgeneration;
					aemo_history[aemo_idx].semischeduledgeneration = aemo_data.semischeduledgeneration;
					aemo_history[aemo_idx].renewables = aemo_data.renewables;
					aemo_history[aemo_idx].valid = true;

					if (++aemo_idx >= HISTORY)
						aemo_idx = 0;

					/* Success, go back to IDLE */
					state = IDLE;
				}
				
				/* No luck, we will try again in five */
				vTaskDelay(5000 / portTICK_PERIOD_MS);
				break;
		}
		vTaskDelay(1000 / portTICK_PERIOD_MS);
	}

	if (timeinfo.tm_sec == 0) parse_time(&timeinfo);
	vTaskDelay(1000 / portTICK_PERIOD_MS);
}

void parse_time(const struct tm *timeinfo)
{
	printf("Time: %02d:%02d:%02d ",timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);

	printf("Network Tarrif: ");

	if ((timeinfo->tm_hour >= 10) && (timeinfo->tm_hour < 15)) {
		printf("Solar Sponge\r\n");
		gpio_set_level(GPIO_OUT1, 1);
	}
	else if ((timeinfo->tm_hour >= 1)  && (timeinfo->tm_hour <= 5)) {
		printf("Off-Peak\r\n");
		gpio_set_level(GPIO_OUT1, 0);
	}
	else {
		printf("Peak\r\n");
		gpio_set_level(GPIO_OUT1, 0);
	}
}