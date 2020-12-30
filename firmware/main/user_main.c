#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"

#include "lwip/sockets.h"

static const char *TAG = "app";
static EventGroupHandle_t wifi_event_group;
const int CONNECTED_BIT = BIT0;


static void send_udp_packet()
{
	char payload[128];
	static int counter = 0;
	struct sockaddr_in destAddr;
	destAddr.sin_addr.s_addr = inet_addr(CONFIG_SERVER_IP);
	destAddr.sin_family = AF_INET;
	destAddr.sin_port = htons(CONFIG_SERVER_UDP_PORT);

	int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
	if (sock < 0) {
		ESP_LOGE(TAG, "Unable to create socket: %s/%d", strerror(errno), errno);
		return;
	}

	sprintf(payload, "test %d\n", counter++);
	int err = sendto(sock, payload, strlen(payload), 0,
	                 (struct sockaddr *)&destAddr, sizeof(destAddr));
	if (err < 0) {
		ESP_LOGE(TAG, "Error occured during sending: %s/%d", strerror(errno), errno);
		shutdown(sock, 0);
		close(sock);
		return;
	}
	ESP_LOGI(TAG, "Message %d sent", counter-1);

	shutdown(sock, 0);
	close(sock);
}


static esp_err_t event_handler(void* ctx, system_event_t* event)
{
	switch (event->event_id) {
		case SYSTEM_EVENT_STA_START:
			ESP_LOGI(TAG, "SYSTEM_EVENT_STA_START");
			ESP_ERROR_CHECK(esp_wifi_connect());
			break;

		case SYSTEM_EVENT_STA_CONNECTED:
			ESP_LOGI(TAG, "SYSTEM_EVENT_STA_CONNECTED");
			break;

		case SYSTEM_EVENT_STA_GOT_IP:
			ESP_LOGI(TAG, "SYSTEM_EVENT_STA_GOT_IP");
			xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
			break;

		case SYSTEM_EVENT_STA_STOP:
			ESP_LOGI(TAG, "SYSTEM_EVENT_STA_STOP");
			break;

		case SYSTEM_EVENT_STA_LOST_IP:
			ESP_LOGI(TAG, "SYSTEM_EVENT_STA_LOST_IP");
			xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
			break;

		case SYSTEM_EVENT_STA_DISCONNECTED:
			ESP_LOGI(TAG, "SYSTEM_EVENT_STA_DISCONNECTED");
			ESP_ERROR_CHECK(esp_wifi_connect());
			xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
			break;

		default:
			ESP_LOGW(TAG, "Unhandled Event: %d", event->event_id);
			break;
	}

	return ESP_OK;
}


static void start_wifi() {

	tcpip_adapter_init();

	wifi_event_group = xEventGroupCreate();

	ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));
	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));
	ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

	wifi_config_t wifi_config = {
	    .sta = {
	        .ssid = CONFIG_WIFI_SSID,
	        .password = CONFIG_WIFI_PASSWORD,
	    },
	};
	ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
	ESP_ERROR_CHECK(esp_wifi_start());

	// wait for being connected
	xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);
}


void app_main(void)
{
	// Initialize NVS.
	esp_err_t err = nvs_flash_init();
	if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
		// OTA app partition table has a smaller NVS partition size than the
		// non-OTA partition table. This size mismatch may cause NVS
		// initialization to fail.If this happens, we erase NVS partition and
		// initialize NVS again.
		ESP_ERROR_CHECK(nvs_flash_erase());
		err = nvs_flash_init();
	}
	ESP_ERROR_CHECK( err );

	start_wifi();

	while (1) {

		for (int cnt=0; cnt < 2; cnt++) {
			send_udp_packet();
			sleep(1);
		}

		ESP_ERROR_CHECK( esp_wifi_stop() );

		ESP_ERROR_CHECK( esp_sleep_enable_timer_wakeup(5*1000*1000) );

		ESP_LOGE(TAG, " ---> enter light sleep");
		ESP_ERROR_CHECK( esp_light_sleep_start() );
		ESP_LOGE(TAG, " ---> returned from light sleep");

		ESP_ERROR_CHECK( esp_wifi_start() );

		sleep(6);
	}
}
