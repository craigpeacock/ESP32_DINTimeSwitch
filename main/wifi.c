#include <stdio.h>
#include <string.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_sleep.h"
#include "spi_flash_mmap.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_eth.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"

#define CONFIG_ESP_WIFI_SSID "test_ap"
#define CONFIG_ESP_WIFI_PASSWORD "secretsquirrel"
#define CONFIG_ESP_MAXIMUM_RETRY 0

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT	BIT0
#define WIFI_FAIL_BIT		BIT1

static const char *TAG = "WIFI";

static int s_retry_num = 0;

static esp_ip6_addr_t s_ipv6_addr;

/* Types of ipv6 addresses to be displayed on ipv6 events */
static const char *s_ipv6_addr_types[] = {
	"UNKNOWN",
	"GLOBAL",
	"LINK_LOCAL",
	"SITE_LOCAL",
	"UNIQUE_LOCAL",
	"IPV4_MAPPED_IPV6"
};

static void event_handler(void *esp_netif, esp_event_base_t event_base,
	int32_t event_id, void* event_data)
{
	if (event_base == WIFI_EVENT) {
		switch (event_id) {
			case WIFI_EVENT_STA_START:
				esp_wifi_connect();
				break;

			case WIFI_EVENT_STA_CONNECTED:
				//Create interface link-local IPv6 address for WiFi Station. 
				esp_netif_create_ip6_linklocal(esp_netif);
				break;

			case WIFI_EVENT_STA_DISCONNECTED:
				if (s_retry_num < CONFIG_ESP_MAXIMUM_RETRY) {
					esp_wifi_connect();
					s_retry_num++;
					ESP_LOGI(TAG, "retry to connect to the AP");
				} else {
					xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
				}
				break;

			default:
				break;
		}
	}

	if (event_base == IP_EVENT) {
		switch (event_id) {
			case IP_EVENT_STA_GOT_IP:
				ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
				ESP_LOGI(TAG, "IPv4: " IPSTR, IP2STR(&event->ip_info.ip));
				s_retry_num = 0;
				xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);	
				break;

			case IP_EVENT_GOT_IP6:
				ip_event_got_ip6_t* ipv6event = (ip_event_got_ip6_t*) event_data;
				esp_ip6_addr_type_t ipv6_type = esp_netif_ip6_get_addr_type(&ipv6event->ip6_info.ip);
				ESP_LOGI(TAG, "IPv6: " IPV6STR " (%s)", IPV62STR(ipv6event->ip6_info.ip), s_ipv6_addr_types[ipv6_type]);
				break;
		}
	}
}

void wifi_init_sta(void)
{
	//Initialize NVS
	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);

	s_wifi_event_group = xEventGroupCreate();

	ESP_ERROR_CHECK(esp_netif_init());

	ESP_ERROR_CHECK(esp_event_loop_create_default());
	esp_netif_t *netif = esp_netif_create_default_wifi_sta();

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, netif));
	ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));
	ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_GOT_IP6, &event_handler, NULL));

	wifi_config_t wifi_config = {
		.sta = {
			.ssid = CONFIG_ESP_WIFI_SSID,
			.password = CONFIG_ESP_WIFI_PASSWORD,
			/* Setting a password implies station will connect to all security modes including WEP/WPA.
			 * However these modes are deprecated and not advisable to be used. Incase your Access point
			 * doesn't support WPA2, these mode can be enabled by commenting below line */
			.threshold.authmode = WIFI_AUTH_WPA2_PSK,
			.pmf_cfg = {
				.capable = true,
				.required = false
			},
		},
	};
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
	ESP_ERROR_CHECK(esp_wifi_start());

	/* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
	 * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
	EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
		WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
		pdFALSE,
		pdFALSE,
		portMAX_DELAY);

	/* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
	 * happened. */
	if (bits & WIFI_CONNECTED_BIT) {
		ESP_LOGI(TAG, "Connected to AP SSID: %s", CONFIG_ESP_WIFI_SSID);
	} else if (bits & WIFI_FAIL_BIT) {
		ESP_LOGI(TAG, "Failed to connect to SSID: %s", CONFIG_ESP_WIFI_SSID);
	} else {
		ESP_LOGE(TAG, "Unexpected event");
	}

	vEventGroupDelete(s_wifi_event_group);
}
