#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "esp_http_client.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_crt_bundle.h"
#include "cJSON.h"

#include "defines.h"
#include "aemo.h"

static const char *TAG = "aemo";

struct aemo aemo_data;

// The PEM file was extracted from the output of this command:
// openssl s_client -showcerts -connect visualisations.aemo.com.au:443 </dev/null

extern const char server_root_cert_pem_start[] asm("_binary_server_root_cert_pem_start");
extern const char server_root_cert_pem_end[]   asm("_binary_server_root_cert_pem_end");

void parse_aemo_json(char *ptr, struct aemo *aemo_data)
{
	const cJSON *regions;
	const cJSON *parameter;

	cJSON *NEM = cJSON_Parse(ptr);

	regions = cJSON_GetObjectItemCaseSensitive(NEM, "ELEC_NEM_SUMMARY");

	cJSON_ArrayForEach(parameter, regions)
	{
		cJSON *name = cJSON_GetObjectItemCaseSensitive(parameter, "REGIONID");
		//ESP_LOGI(TAG, Region %s\n",name->valuestring);
		if (strcmp(name->valuestring, aemo_data->region) == 0) {
			cJSON *settlement = cJSON_GetObjectItemCaseSensitive(parameter, "SETTLEMENTDATE");
			if (settlement != NULL) {
				/* String in the format of 2020-12-19T15:10:00 */
					if (strptime((char *)settlement->valuestring, "%Y-%m-%dT%H:%M:%S", &aemo_data->settlement) == NULL)
						ESP_LOGE(TAG, "Unable to parse settlement time\r\n");
			}
			cJSON *price = cJSON_GetObjectItemCaseSensitive(parameter, "PRICE");
			cJSON *totaldemand = cJSON_GetObjectItemCaseSensitive(parameter, "TOTALDEMAND");
			cJSON *netinterchange = cJSON_GetObjectItemCaseSensitive(parameter, "NETINTERCHANGE");
			cJSON *scheduledgeneration = cJSON_GetObjectItemCaseSensitive(parameter, "SCHEDULEDGENERATION");
			cJSON *semischeduledgeneration = cJSON_GetObjectItemCaseSensitive(parameter, "SEMISCHEDULEDGENERATION");

			aemo_data->price = price->valuedouble;
			aemo_data->totaldemand = totaldemand->valuedouble;
			aemo_data->netinterchange = netinterchange->valuedouble;
			aemo_data->scheduledgeneration = scheduledgeneration->valuedouble;
			aemo_data->semischeduledgeneration = semischeduledgeneration->valuedouble;
			aemo_data->valid = true;
		}
	}
	cJSON_Delete(NEM);

	// Calculate renewables
	aemo_data->renewables = (aemo_data->semischeduledgeneration / (aemo_data->totaldemand)) * 100;
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
			if (strcmp(evt->header_key, "Date") == 0) 
				ESP_LOGI(TAG, "Header %s: %s",evt->header_key,evt->header_value);
			if (strcmp(evt->header_key, "Expires") == 0) 
				ESP_LOGI(TAG, "Header %s: %s",evt->header_key,evt->header_value);
			if (strcmp(evt->header_key, "Last-Modified") == 0) 
				ESP_LOGI(TAG, "Header %s: %s",evt->header_key,evt->header_value);
			break;
		case HTTP_EVENT_ON_DATA:
			if (!esp_http_client_is_chunked_response(evt->client)) {
				if (evt->user_data) {
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

void aemo_get_price(struct aemo *aemo_data)
{
	int status_code;
	int content_len;
	char *buffer = NULL;

	buffer = malloc(32768);

	ESP_LOGI(TAG, "Fetching AEMO spot price");

	esp_http_client_config_t config = {
		.url = "https://visualisations.aemo.com.au/aemo/apps/api/report/ELEC_NEM_SUMMARY",
		.event_handler = _http_event_handle,
		.cert_pem = server_root_cert_pem_start,
		.user_data = buffer,
		};

	esp_http_client_handle_t client = esp_http_client_init(&config);

	esp_err_t err = esp_http_client_perform(client);

	if (err == ESP_OK) {
		status_code = esp_http_client_get_status_code(client);
		content_len = esp_http_client_get_content_length(client);
		ESP_LOGI(TAG, "Status = %d, Content length = %d bytes", status_code, content_len);
		parse_aemo_json(buffer, aemo_data);
		free(buffer);
	}
	esp_http_client_cleanup(client);
}
