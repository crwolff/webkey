/* Simple HTTP Server Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_eth.h"

#include <esp_http_server.h>

extern const char *TAG;

static const char homepage[] =
			"<!DOCTYPE html>\n"
			"<html>\n"
			"<head>\n"
			"<style>\n"
			".button {\n"
			"  border: none;\n"
			"  color: white;\n"
			"  padding: 16px 32px;\n"
			"  text-align: center;\n"
			"  text-decoration: none;\n"
			"  display: inline-block;\n"
			"  font-size: 16px;\n"
			"  margin: 4px 2px;\n"
			"  transition-duration: 0.4s;\n"
			"  cursor: pointer;\n"
			"}\n"
			".button1 {\n"
			"  background-color: white; \n"
			"  color: black; \n"
			"  border: 2px solid #4CAF50;\n"
			"}\n"
			".button1:hover {\n"
			"  background-color: #4CAF50;\n"
			"  color: white;\n"
			"}\n"
			".button2 {\n"
			"  background-color: white; \n"
			"  color: black; \n"
			"  border: 2px solid #008CBA;\n"
			"}\n"
			".button2:hover {\n"
			"  background-color: #008CBA;\n"
			"  color: white;\n"
			"}\n"
			"</style>\n"
			"</head>\n"
			"<body>\n"
			"<h1>Select boot sequence</h1>\n"
			"<button class=\"button button1\">Windows</button>\n"
			"<button class=\"button button2\">Linux</button>\n"
			"</body>\n"
			"</html>\n";

/* Our URI handler function to be called during GET /uri request */
static esp_err_t get_handler(httpd_req_t *req)
{
    httpd_resp_send(req, homepage, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

/* URI handler structure for GET /uri */
static httpd_uri_t uri_get = {
    .uri      = "/",
    .method   = HTTP_GET,
    .handler  = get_handler,
    .user_ctx = NULL
};

static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &uri_get);
        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}

static void stop_webserver(httpd_handle_t server)
{
    // Stop the httpd server
    httpd_stop(server);
}

static void disconnect_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server) {
        ESP_LOGI(TAG, "Stopping webserver");
        stop_webserver(*server);
        *server = NULL;
    }
}

static void connect_handler(void* arg, esp_event_base_t event_base,
                            int32_t event_id, void* event_data)
{
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server == NULL) {
        ESP_LOGI(TAG, "Starting webserver");
        *server = start_webserver();
    }
}


void server_init(void)
{
    static httpd_handle_t server = NULL;

    /* Register event handlers to stop the server when Wi-Fi
     * and re-start it upon connection.
     */
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &connect_handler, &server));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &disconnect_handler, &server));

    /* Start the server for the first time */
    server = start_webserver();
}
