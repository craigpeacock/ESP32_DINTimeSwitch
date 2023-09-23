
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "esp_log.h"
#include "esp_sntp.h"

static const char *TAG = "ntp";

void time_sync_notification_cb(struct timeval *tv)
{
	ESP_LOGI(TAG, "Notification of a time synchronization event");
}

void ntp_get_time(void)
{
	// Initialise NTP
	sntp_setoperatingmode(SNTP_OPMODE_POLL);
	sntp_setservername(0, "time.windows.com");
	sntp_set_time_sync_notification_cb(time_sync_notification_cb);
	sntp_init();

	// Wait for time to be set
	int retry = 0;
	const int max_retry = 10;

	while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < max_retry) {
		ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, max_retry);
		vTaskDelay(2000 / portTICK_PERIOD_MS);
	}
}
