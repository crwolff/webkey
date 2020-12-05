/* Simple HTTP Server Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <esp_log.h>
#include <esp_system.h>
#include <sys/param.h>
#include <esp_spi_flash.h>
#include <esp_partition.h>
#include "esp_ota_ops.h"

/* Should put these in .h file(s) */
extern const char *TAG;

/* Local storage */
static int curr_addr;

/* Setup for OTA operation */
esp_err_t ota_init(void)
{
    ESP_LOGI(TAG, "OTA_INIT() called");
    curr_addr = 0;
    return ESP_OK;
}

/* Write a chunk of data */
esp_err_t ota_write(char *buf, int len)
{
    ESP_LOGI(TAG, "OTA_WRITE() called %08X (%d)", curr_addr, len);
    curr_addr += len;
    return ESP_OK;
}

/* Finalize the OTA operation */
esp_err_t ota_finish(esp_err_t err)
{
    ESP_LOGI(TAG, "OTA_FINISH() called");
    return err;
}
