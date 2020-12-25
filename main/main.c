/* Simple HTTP Server Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <esp_log.h>
#include <nvs_flash.h>

const char *TAG = "webkey";

extern int32_t web_server_down;

/* Forware declaration */
void wifi_init_sta( int32_t, const char *, const char * );
void server_init(void);
void usb_init(void);

/* Main application */
void app_main(void)
{
    size_t nvs_len;
    char wifi_ssid[33];
    char wifi_pass[65];

    // Initialize NVS subsystem
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );

    // Get SSID/password from NVS
    nvs_handle_t nvsHandle;
    err = nvs_open("storage", NVS_READWRITE, &nvsHandle);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(err));
        strlcpy(wifi_ssid, CONFIG_ESP_WIFI_SSID, sizeof(wifi_ssid));
        strlcpy(wifi_pass, CONFIG_ESP_WIFI_PASSWORD, sizeof(wifi_pass));
    } else {
        nvs_len = sizeof(wifi_ssid);
        err = nvs_get_str(nvsHandle, "WIFI_SSID", wifi_ssid, &nvs_len);
        if ( err != ESP_OK ) {
            ESP_LOGI(TAG, "WiFi SSID not set, using default");
            strlcpy(wifi_ssid, CONFIG_ESP_WIFI_SSID, sizeof(wifi_ssid));
        } else
        nvs_len = sizeof(wifi_pass);
        err = nvs_get_str(nvsHandle, "WIFI_PASS", wifi_pass, &nvs_len);
        if ( err != ESP_OK ) {
            ESP_LOGI(TAG, "WiFi password not set, using default");
            strlcpy(wifi_pass, CONFIG_ESP_WIFI_PASSWORD, sizeof(wifi_pass));
        }
        nvs_close(nvsHandle);
    }

    // Start WiFi
    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    wifi_init_sta( 0, wifi_ssid, wifi_pass );

    // Start webserver
    server_init();

    // Start USB
    usb_init();

    // Watch for connection loss and try reconnect
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        if ( web_server_down ) {
            web_server_down = 0;
            wifi_init_sta( 1, wifi_ssid, wifi_pass );
        }
    }
}
