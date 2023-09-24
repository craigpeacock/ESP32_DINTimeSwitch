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

void parse_time(const struct tm *timeinfo);

void app_main(void)
{
	time_t now;
	struct tm timeinfo;

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
	aemo_data.settlement = malloc(25);
	aemo_get_price(&aemo_data);
	ESP_LOGI(TAG, "Settlement Date: %s",aemo_data.settlement);
	ESP_LOGI(TAG, "Price: $%.02f MWh",aemo_data.price);
	ESP_LOGI(TAG, "Total Demand: %.02f MW",aemo_data.totaldemand);
	ESP_LOGI(TAG, "Export: %.02f MW",aemo_data.netinterchange);
	ESP_LOGI(TAG, "Scheduled Generation (Baseload): %.02f MW",aemo_data.scheduledgeneration);
	ESP_LOGI(TAG, "Semi Scheduled Generation (Renewable): %.02f MW",aemo_data.semischeduledgeneration);

	while (1) {

		// Get current time
		time(&now);
		// Populate timeinfo structure
		localtime_r(&now, &timeinfo);

		if (timeinfo.tm_sec == 0) parse_time(&timeinfo);

		vTaskDelay(1000 / portTICK_PERIOD_MS);
	}
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