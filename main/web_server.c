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

/* Handler to respond with home page */
static esp_err_t index_html_get_handler(httpd_req_t *req)
{
    extern const unsigned char index_html_start[] asm("_binary_index_html_start");
    extern const unsigned char index_html_end[]   asm("_binary_index_html_end");
    const size_t index_html_size = (index_html_end - index_html_start);
    httpd_resp_send(req, (const char *)index_html_start, index_html_size);
    return ESP_OK;
}

/* Handler to redirect incoming GET request for / to /index.html
 * This can be overridden by uploading file with same name */
static esp_err_t root_get_handler(httpd_req_t *req)
{
    httpd_resp_set_status(req, "307 Temporary Redirect");
    httpd_resp_set_hdr(req, "Location", "/index.html");
    httpd_resp_send(req, NULL, 0);  // Response body can be empty
    return ESP_OK;
}

/* Handler to respond with an icon file embedded in flash.
 * Browsers expect to GET website icon at URI /favicon.ico.
 * This can be overridden by uploading file with same name */
static esp_err_t favicon_get_handler(httpd_req_t *req)
{
    extern const unsigned char favicon_ico_start[] asm("_binary_favicon_ico_start");
    extern const unsigned char favicon_ico_end[]   asm("_binary_favicon_ico_end");
    const size_t favicon_ico_size = (favicon_ico_end - favicon_ico_start);
    httpd_resp_set_type(req, "image/x-icon");
    httpd_resp_send(req, (const char *)favicon_ico_start, favicon_ico_size);
    return ESP_OK;
}

/* Handler to respond with update page */
static esp_err_t update_html_get_handler(httpd_req_t *req)
{
    extern const unsigned char update_html_start[] asm("_binary_update_html_start");
    extern const unsigned char update_html_end[]   asm("_binary_update_html_end");
    const size_t update_html_size = (update_html_end - update_html_start);
    httpd_resp_send(req, (const char *)update_html_start, update_html_size);
    return ESP_OK;
}

/* Handler to respond to wildcard URI and direct the reponse */
static esp_err_t get_handler(httpd_req_t *req)
{
    /* Return one of a limited number of supported paths */
    if (strcmp(req->uri, "/") == 0) {
        return root_get_handler(req);
    } else if (strcmp(req->uri, "/index.html") == 0) {
        return index_html_get_handler(req);
    } else if (strcmp(req->uri, "/favicon.ico") == 0) {
        return favicon_get_handler(req);
    } else if (strcmp(req->uri, "/update.html") == 0) {
        return update_html_get_handler(req);
    }

    /* Respond with 404 Not Found */
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File does not exist");
    return ESP_FAIL;
}

/* URI handler structure for GET */
static httpd_uri_t uri_get = {
    .uri      = "/*",
    .method   = HTTP_GET,
    .handler  = get_handler,
    .user_ctx = NULL
};

/* Handler for POST action */
extern uint32_t button_pressed;
static esp_err_t post_handler(httpd_req_t *req)
{
    char buf[100];
    char *resp;
    int ret, remaining = req->content_len;

    // Read any posted data
    while (remaining > 0) {
        /* Read the data for the request */
        if ((ret = httpd_req_recv(req, buf,
                        MIN(remaining, sizeof(buf)))) <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                /* Retry receiving if timeout occurred */
                continue;
            }
            return ESP_FAIL;
        }
        remaining -= ret;

        /* Log data received */
        ESP_LOGI(TAG, "=========== RECEIVED DATA ==========");
        ESP_LOGI(TAG, "%.*s", ret, buf);
        ESP_LOGI(TAG, "====================================");
    }

    // Trigger the USB task
    if ( button_pressed == 0 ) {
        ESP_LOGI(TAG, "POST: %s", req->uri);
        if (strcmp(req->uri, "/ctrl?key=b1") == 0) {
            button_pressed = 1;
            resp = "Okay\n";
        } else if (strcmp(req->uri, "/ctrl?key=b2") == 0) {
            button_pressed = 2;
            resp = "Okay\n";
        } else if (strcmp(req->uri, "/ctrl?key=b3") == 0) {
            button_pressed = 3;
            resp = "Okay\n";
        } else if (strcmp(req->uri, "/ctrl?key=b4") == 0) {
            button_pressed = 4;
            resp = "Okay\n";
        } else {
            resp = "Bad Selection\n";
        }
    } else {
        resp = "Busy\n";
    }

    // Send response
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static const httpd_uri_t uri_post = {
    .uri       = "/ctrl",
    .method    = HTTP_POST,
    .handler   = post_handler,
    .user_ctx  = NULL
};

/* Start up the webserver */
static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    // Start the httpd server
    config.uri_match_fn = httpd_uri_match_wildcard;
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &uri_get);
        httpd_register_uri_handler(server, &uri_post);
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
