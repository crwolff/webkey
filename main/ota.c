/* Simple HTTP Server Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <esp_log.h>
#include <esp_system.h>
#include <sys/param.h>
#include "esp_partition.h"
#include "esp_ota_ops.h"

/* Should put these in .h file(s) */
extern const char *TAG;

/* Local storage */
static const esp_partition_t *update_partition = NULL;
static esp_ota_handle_t update_handle = 0;

/* Setup for OTA operation */
esp_err_t ota_init(void)
{
    esp_err_t err;

    update_partition = esp_ota_get_next_update_partition(NULL);
    if ( update_partition == NULL ) {
        ESP_LOGI(TAG, "Error: update_partition is NULL");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Writing to partition subtype %d at offset 0x%x",
             update_partition->subtype, update_partition->address);
    err = esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES, &update_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed (%s)", esp_err_to_name(err));
        return err;
    }
    return ESP_OK;
}

/* Write a chunk of data */
esp_err_t ota_write(char *buf, int len)
{
    return esp_ota_write( update_handle, (const void *)buf, len);
}

/* Finalize the OTA operation */
esp_err_t ota_finish(esp_err_t old_err)
{
    esp_err_t err;

    ESP_LOGI(TAG, "Update writing complete");
    err = esp_ota_end(update_handle);
    if (err != ESP_OK) {
        if (err == ESP_ERR_OTA_VALIDATE_FAILED) {
            ESP_LOGE(TAG, "Image validation failed, image is corrupted");
        }
        ESP_LOGE(TAG, "esp_ota_end failed (%s)!", esp_err_to_name(err));
        return err;
    }

    if ( old_err == ESP_OK ) {
        err = esp_ota_set_boot_partition(update_partition);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_set_boot_partition failed (%s)!", esp_err_to_name(err));
            return err;
        }
        //ESP_LOGI(TAG, "Prepare to restart system!");
        //esp_restart();
    }
    ESP_LOGI(TAG, "Update finished");

    // If an error was passed in, return it
    return old_err;
}
