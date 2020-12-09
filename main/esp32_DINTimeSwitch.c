#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "esp_event.h"
#include "driver/gpio.h"

#include "wifi.h"
#include "ntp.h"
#include "aemo.h"

#define GPIO_OUT1	16
#define GPIO_OUT2	17
#define GPIO_OUTPUT_PIN_SEL  ((1ULL<<GPIO_OUT1) | (1ULL<<GPIO_OUT2))

void parse_time(const struct tm *timeinfo);
void init_gpio(void);

void app_main(void)
{
	time_t now;
	struct tm timeinfo;

	// Set timezone to Adelaide Time
	setenv("TZ", "ACST-9:30ACDT,M10.1.0,M4.1.0/3", 1);
	tzset();

	// Initialise GPIO
	init_gpio();
	// Initialise network
	wifi_init_sta();
	// Initialise network time
	ntp_get_time();

	aemo_get_price();

	while (1) {

		// Get current time
		time(&now);
		// Populate timeinfo structure
		localtime_r(&now, &timeinfo);

		if (timeinfo.tm_sec == 0) parse_time(&timeinfo);

		sleep(1);
	}
}

void parse_time(const struct tm *timeinfo)
{
	//char strftime_buf[64];
	//strftime(strftime_buf, sizeof(strftime_buf), "%c", timeinfo);
	//printf("Time: %s\n", strftime_buf);

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

void init_gpio(void)
{
	gpio_config_t io_conf;
	//disable interrupt
	io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
	//set as output mode
	io_conf.mode = GPIO_MODE_OUTPUT;
	//bit mask of the pins that you want to set,e.g.GPIO18/19
	io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
	//disable pull-down mode
	io_conf.pull_down_en = 0;
	//disable pull-up mode
	io_conf.pull_up_en = 0;
	//configure GPIO with the given settings
	gpio_config(&io_conf);
}
