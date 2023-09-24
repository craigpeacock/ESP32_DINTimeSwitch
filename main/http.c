
#include <freertos/FreeRTOS.h>
#include <esp_http_server.h>
#include <freertos/task.h>
#include <sys/param.h>
#include <esp_ota_ops.h>
#include <time.h>
#include "esp_flash.h"
#include "esp_log.h"
#include "cJSON.h"

#include "defines.h"
#include "http.h"
#include "gpio.h"
#include "aemo.h"

static const char *TAG = "http";

static httpd_handle_t http_server = NULL;

/*
 * Serve OTA update portal (index.html)
 */
extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_index_html_end");

extern const uint8_t style_css_start[] asm("_binary_style_css_start");
extern const uint8_t style_css_end[] asm("_binary_style_css_end");

esp_err_t index_get_handler(httpd_req_t *req)
{
	httpd_resp_send(req, (const char *) index_html_start, index_html_end - index_html_start);
	return ESP_OK;
}

esp_err_t css_get_handler(httpd_req_t *req)
{
	httpd_resp_send(req, (const char *) style_css_start, style_css_end - style_css_start);
	return ESP_OK;
}

esp_err_t restart_post_handler(httpd_req_t *req)
{
	char content[100];

	// Truncate if content length larger than the buffer
	size_t recv_size = MIN(req->content_len, sizeof(content));

	int ret = httpd_req_recv(req, content, recv_size);
	// 0 return value indicates connection closed
	if (ret <= 0) { 
		// Check if timeout occurred
		if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
			// In case of timeout one can choose to retry calling
			// httpd_req_recv(), but to keep it simple, here we
			// respond with an HTTP 408 (Request Timeout) error
			httpd_resp_send_408(req);
		}
		// In case of error, returning ESP_FAIL will
		// ensure that the underlying socket is closed.
		return ESP_FAIL;
	}

	const char resp[] = "URI POST Response";
	httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);

	ESP_LOGI(TAG, "Received [%.*s]", ret, content);

	if(content[0] == '1') {
		// If we stop http server, we kill this process.
		//http_server_stop();
		vTaskDelay(500 / portTICK_PERIOD_MS);
		esp_restart();
	}

	return ESP_OK;
}

esp_err_t firmware_upgrade_post_handler(httpd_req_t *req)
{
	char content[8092];
	esp_ota_handle_t ota_handle;
	int remaining = req->content_len;
	int ret;

	const esp_partition_t *ota_partition = esp_ota_get_next_update_partition(NULL);
	ESP_ERROR_CHECK(esp_ota_begin(ota_partition, OTA_SIZE_UNKNOWN, &ota_handle));

	while (remaining > 0) {
		int recv_len = httpd_req_recv(req, content, MIN(remaining, sizeof(content)));

		// Timeout Error: Just retry
		if (recv_len == HTTPD_SOCK_ERR_TIMEOUT) {
			continue;

		// Serious Error: Abort OTA
		} else if (recv_len <= 0) {
			httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Protocol Error");
			return ESP_FAIL;
		}

		// Successful Upload: Flash firmware chunk
		ESP_LOGI(TAG, "Flashing chunk (%d bytes)", recv_len);
		ret = esp_ota_write(ota_handle, (const void *)content, recv_len);
		if (ret != ESP_OK) {
			switch (ret) {
				case ESP_ERR_INVALID_ARG:
					ESP_LOGE(TAG, "OTA handle is invalid.");
					httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA handle invalid");
					break;

				case ESP_ERR_OTA_VALIDATE_FAILED:
					ESP_LOGE(TAG, "Firmware magic (header) invalid.");
					httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Invalid magic");
					break;

#if 0
				case ESP_ERR_FLASH_OP_TIMEOUT:
				case ESP_ERR_FLASH_OP_FAIL:
					ESP_LOGE(TAG, "Flash write failed.");
					httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Flash write error");
					break;
#endif
				case ESP_ERR_OTA_SELECT_INFO_INVALID:
					ESP_LOGE(TAG, "OTA data partition has invalid contents.");
					httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Flash error");
					break;

				default:
					ESP_LOGE(TAG, "Unknown OTA upgrade error");
					httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Unknown error");
					break;
			}
			return ESP_FAIL;
		}

		remaining -= recv_len;
	}

	// Validate, switch to new OTA image and reboot
	if (esp_ota_end(ota_handle) != ESP_OK || esp_ota_set_boot_partition(ota_partition) != ESP_OK) {
		httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Validation / Activation Error");
		return ESP_FAIL;
	}

	ESP_LOGI(TAG, "Firmware update complete, rebooting now");
	httpd_resp_sendstr(req, "Firmware update complete, rebooting now\n");

	vTaskDelay(500 / portTICK_PERIOD_MS);

	esp_restart();

	return ESP_OK;
}

esp_err_t firmware_version_handler(httpd_req_t *req) 
{
	char fwver[10];
	sprintf(fwver, "%s\r\n", VERSION);
	httpd_resp_set_type(req, "text/plain");
	httpd_resp_send(req, fwver, strlen(fwver));
	return ESP_OK;
}

esp_err_t read_status_get_handler(httpd_req_t *req)
{
	time_t now;
	struct tm timeinfo;
	char buf[32];

	// Check if the request method is GET
	if (req->method != HTTP_GET)
	{
		httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Only GET method allowed");
		return ESP_FAIL;
	}

	time(&now);
	localtime_r(&now, &timeinfo);

	// Create a JSON object
	cJSON *root = cJSON_CreateObject();

	strftime(buf, sizeof(buf), "%A %B %d %Y", &timeinfo);
	cJSON_AddStringToObject(root, "date", buf);

	strftime(buf, sizeof(buf), "%I:%M:%S %p %Z", &timeinfo);
	cJSON_AddStringToObject(root, "time", buf);	

	sprintf(buf, "Solar Sponge");
	cJSON_AddStringToObject(root, "tarrif", buf);	

	sprintf(buf, "$%.02f", aemo_data.price);
	cJSON_AddStringToObject(root, "price", buf);

	sprintf(buf, "%.01f", aemo_data.renewables);
	cJSON_AddStringToObject(root, "renewables", buf);

	sprintf(buf, "%s", gpio_read_output1() ? "ON": "OFF");
	cJSON_AddStringToObject(root, "output1", buf);

	sprintf(buf, "%s", gpio_read_output2() ? "ON": "OFF");
	cJSON_AddStringToObject(root, "output2", buf);

	// Convert the JSON object to a string
	char *jsonString = cJSON_Print(root);
	
	// Set the response content type
	httpd_resp_set_type(req, "application/json");

	// Send the sample JSON data as the response body
	httpd_resp_send(req, jsonString, strlen(jsonString));

	cJSON_Delete(root);

	return ESP_OK;
}

httpd_uri_t index_get = {
	.uri		= "/",
	.method		= HTTP_GET,
	.handler	= index_get_handler,
	.user_ctx	= NULL
};

httpd_uri_t ccs_get = {
	.uri		= "/style.css",
	.method		= HTTP_GET,
	.handler	= css_get_handler,
	.user_ctx	= NULL
};

httpd_uri_t restart_post = {
	.uri		= "/restart",
	.method		= HTTP_POST,
	.handler	= restart_post_handler,
	.user_ctx	= NULL
};

httpd_uri_t firmware_upgrade_post = {
	.uri		= "/firmware_upgrade",
	.method		= HTTP_POST,
	.handler	= firmware_upgrade_post_handler,
	.user_ctx	= NULL
};

httpd_uri_t fwver_get = {
	.uri	  = "/firmware_version",
	.method   = HTTP_GET,
	.handler  = firmware_version_handler,
	.user_ctx = NULL
};

httpd_uri_t status_get = {
	.uri	  = "/read_status",
	.method   = HTTP_GET,
	.handler  = read_status_get_handler,
	.user_ctx = NULL
};

esp_err_t http_server_init(void)
{
	httpd_config_t config = HTTPD_DEFAULT_CONFIG();

	ESP_LOGI(TAG, "Starting HTTP server on port %d", config.server_port);
	if (httpd_start(&http_server, &config) == ESP_OK) {
		httpd_register_uri_handler(http_server, &index_get);
		httpd_register_uri_handler(http_server, &ccs_get);
		httpd_register_uri_handler(http_server, &restart_post);
		httpd_register_uri_handler(http_server, &firmware_upgrade_post);
		httpd_register_uri_handler(http_server, &fwver_get);
		httpd_register_uri_handler(http_server, &status_get);
	}

	return http_server == NULL ? ESP_FAIL : ESP_OK;
}

esp_err_t http_server_stop(void)
{
	ESP_LOGI(TAG, "Stopping HTTP server");
	int ret;
	if (http_server) {
		ret = httpd_stop(http_server);
	} else {
		ret = ESP_ERR_INVALID_ARG;
	}
	return ret;
}
