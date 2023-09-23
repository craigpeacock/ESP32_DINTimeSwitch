#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "esp_http_client.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_crt_bundle.h"
#include "cJSON.h"

static const char *TAG = "HTTP/AEMO";

#define WEB_SERVER "aemo.com.au"
#define WEB_PORT "443"
#define WEB_URL "https://aemo.com.au/aemo/apps/api/report/ELEC_NEM_SUMMARY"

extern const char server_root_cert_pem_start[] asm("_binary_server_root_cert_pem_start");
extern const char server_root_cert_pem_end[]   asm("_binary_server_root_cert_pem_end");

#define REGION "SA1"

void parse_aemo_json(char *ptr)
{
	const cJSON *regions;
	const cJSON *parameter;

	cJSON *NEM = cJSON_Parse(ptr);

	regions = cJSON_GetObjectItemCaseSensitive(NEM, "ELEC_NEM_SUMMARY");

	cJSON_ArrayForEach(parameter, regions)
	{
		cJSON *name = cJSON_GetObjectItemCaseSensitive(parameter, "REGIONID");
		//printf("Region %s\n",name->valuestring);
		if (strcmp(name->valuestring, REGION) == 0) {
			cJSON *settlement = cJSON_GetObjectItemCaseSensitive(parameter, "SETTLEMENTDATE");
			printf("South Australia %s\r\n",settlement->valuestring);
			cJSON *price = cJSON_GetObjectItemCaseSensitive(parameter, "PRICE");
			cJSON *totaldemand = cJSON_GetObjectItemCaseSensitive(parameter, "TOTALDEMAND");
			cJSON *netinterchange = cJSON_GetObjectItemCaseSensitive(parameter, "NETINTERCHANGE");
			cJSON *scheduledgeneration = cJSON_GetObjectItemCaseSensitive(parameter, "SCHEDULEDGENERATION");
			cJSON *semischeduledgeneration = cJSON_GetObjectItemCaseSensitive(parameter, "SEMISCHEDULEDGENERATION");

			printf("Price: $%.02f MWh\r\n",price->valuedouble);
			printf("Total Demand: %.02f MW\r\n",totaldemand->valuedouble);
			printf("Export: %.02f MW\r\n",netinterchange->valuedouble);
			printf("Scheduled Generation (Baseload): %.02f MW\r\n",scheduledgeneration->valuedouble);
			printf("Semi Scheduled Generation (Renewable): %.02f MW\r\n",semischeduledgeneration->valuedouble);
		}
	}
	cJSON_Delete(NEM);
}

esp_err_t _http_event_handle(esp_http_client_event_t *evt)
{
	static int output_len;

	switch(evt->event_id) {
		case HTTP_EVENT_ERROR:
			//ESP_LOGI(TAG, "HTTP_EVENT_ERROR");
			break;
		case HTTP_EVENT_ON_CONNECTED:
			//ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
			break;
		case HTTP_EVENT_HEADER_SENT:
			//ESP_LOGI(TAG, "HTTP_EVENT_HEADER_SENT");
			break;
		case HTTP_EVENT_ON_HEADER:
			//ESP_LOGI(TAG, "HTTP_EVENT_ON_HEADER %s:%s\r\n",evt->header_key,evt->header_value);
			if (strcmp(evt->header_key, "Date") == 0) printf("Header %s:%s\r\n",evt->header_key,evt->header_value);
			if (strcmp(evt->header_key, "Expires") == 0) printf("Header %s:%s\r\n",evt->header_key,evt->header_value);
			if (strcmp(evt->header_key, "Last-Modified") == 0) printf("Header %s:%s\r\n",evt->header_key,evt->header_value);
			break;
		case HTTP_EVENT_ON_DATA:
			//ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
			if (!esp_http_client_is_chunked_response(evt->client)) {
			if (evt->user_data) {
				//printf("Copying chunk\r\n");
				memcpy(evt->user_data + output_len, evt->data, evt->data_len);
				}
			output_len += evt->data_len;
			}
			break;
		case HTTP_EVENT_ON_FINISH:
			//ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");
			output_len = 0;
			break;
		case HTTP_EVENT_DISCONNECTED:
			//ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
			break;
		case HTTP_EVENT_REDIRECT:
			break;
	}
	return ESP_OK;
}

void aemo_get_price(void)
{
	int status_code;
	int content_len;
	char *buffer = NULL;

	time_t now;
	struct tm timeinfo;
	char strftime_buf[64];

	buffer = malloc(32768);

	while (1) {

		printf("\r\nGetting AEMO Spot Price - ");

		esp_http_client_config_t config = {
			.url = "https://aemo.com.au/aemo/apps/api/report/ELEC_NEM_SUMMARY",
			.event_handler = _http_event_handle,
			.cert_pem = server_root_cert_pem_start,
			.user_data = buffer,
			};

		esp_http_client_handle_t client = esp_http_client_init(&config);

		time(&now);
		localtime_r(&now, &timeinfo);
		strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
		printf("Local Time: %s\n", strftime_buf);

		esp_err_t err = esp_http_client_perform(client);

		if (err == ESP_OK) {
			//status_code = esp_http_client_get_status_code(client);
			//content_len = esp_http_client_get_content_length(client);
			//ESP_LOGI(TAG, "Status = %d, content_length = %d", status_code, content_len);
			parse_aemo_json(buffer);
			//free(buffer);
		}
		esp_http_client_cleanup(client);
		sleep(10);
	}
}
