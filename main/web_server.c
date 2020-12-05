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

/* Should put these in .h file(s) */
extern const char *TAG;

esp_err_t ota_init(void);
esp_err_t ota_write(char *, int);
esp_err_t ota_finish(esp_err_t);

/* Handler to respond with home page */
static esp_err_t index_html_get_handler(httpd_req_t *req)
{
    extern const unsigned char index_html_start[] asm("_binary_index_html_start");
    extern const unsigned char index_html_end[]   asm("_binary_index_html_end");
    const size_t index_html_size = (index_html_end - index_html_start);
    httpd_resp_send(req, (const char *)index_html_start, index_html_size);
    return ESP_OK;
}

/* Handler to redirect incoming GET request for / to /index.html */
static esp_err_t root_get_handler(httpd_req_t *req)
{
    httpd_resp_set_status(req, "307 Temporary Redirect");
    httpd_resp_set_hdr(req, "Location", "/index.html");
    httpd_resp_send(req, NULL, 0);  // Response body can be empty
    return ESP_OK;
}

/* Handler to respond with an icon file embedded in flash.
 * Browsers expect to GET website icon at URI /favicon.ico. */
static esp_err_t favicon_get_handler(httpd_req_t *req)
{
    extern const unsigned char favicon_ico_start[] asm("_binary_favicon_ico_start");
    extern const unsigned char favicon_ico_end[]   asm("_binary_favicon_ico_end");
    const size_t favicon_ico_size = (favicon_ico_end - favicon_ico_start);
    httpd_resp_set_type(req, "image/x-icon");
    httpd_resp_send(req, (const char *)favicon_ico_start, favicon_ico_size);
    return ESP_OK;
}

/* Handler to respond with configuration page */
static esp_err_t config_html_get_handler(httpd_req_t *req)
{
    extern const unsigned char config_html_start[] asm("_binary_config_html_start");
    extern const unsigned char config_html_end[]   asm("_binary_config_html_end");
    const size_t config_html_size = (config_html_end - config_html_start);
    httpd_resp_send(req, (const char *)config_html_start, config_html_size);
    return ESP_OK;
}

/* Handler to redirect incoming GET request for /config to /config.html */
static esp_err_t config_get_handler(httpd_req_t *req)
{
    httpd_resp_set_status(req, "307 Temporary Redirect");
    httpd_resp_set_hdr(req, "Location", "/config.html");
    httpd_resp_send(req, NULL, 0);  // Response body can be empty
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
    } else if (strcmp(req->uri, "/config.html") == 0) {
        return config_html_get_handler(req);
    } else if (strcmp(req->uri, "/config") == 0) {
        return config_get_handler(req);
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

/* Flush posted data */
static esp_err_t flush_post_data(httpd_req_t *req)
{
    char buf[100];
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
    return ESP_OK;
}

/* Handler for ctrl POST action */
extern uint32_t button_pressed;
static esp_err_t ctrl_post_handler(httpd_req_t *req)
{
    char *resp;

    // Clean up any garbage
    if (flush_post_data(req) != ESP_OK)
        return ESP_FAIL;

    // Trigger the USB task
    if ( button_pressed == 0 ) {
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

/* Handler for config POST action */
static esp_err_t config_post_handler(httpd_req_t *req)
{
    char buf[120]; // max=10+64 + 10+32 + 1
    char *token;
    int ret, remaining = req->content_len;

    /* Open NVS */
    nvs_handle_t nvsHandle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &nvsHandle);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(err));
        flush_post_data(req);
        return ESP_FAIL;
    }

    /* Read SSID/Password */
    buf[0] = '\0';
    while (remaining > 0) {
        /* Read the data for the request */
        if ((ret = httpd_req_recv(req, buf,
                        MIN(remaining, sizeof(buf)-1))) <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                /* Retry receiving if timeout occurred */
                continue;
            }
            return ESP_FAIL;
        }
        buf[ret] = '\0';
        remaining -= ret;

        /* Parse received data */
        token = strtok(buf, "&");
        while( token != NULL ) {
            if ( strncmp( token, "wifi_ssid=", 10 ) == 0 ) {
                token += 10;	// Skip key
                if (( strlen(token) > 0 ) && ( strlen(token) <= 32 )) {
                    err = nvs_set_str(nvsHandle, "WIFI_SSID", token);
                    if ( err != ESP_OK ) {
                        ESP_LOGI(TAG, "Error (%s) writing WiFi SSID to NVS", esp_err_to_name(err));
                    }
                }
            }
            else if ( strncmp( token, "wifi_pass=", 10 ) == 0 ) {
                token += 10;	// Skip key
                if (( strlen(token) > 0 ) && ( strlen(token) <= 64 )) {
                    err = nvs_set_str(nvsHandle, "WIFI_PASS", token);
                    if ( err != ESP_OK ) {
                        ESP_LOGI(TAG, "Error (%s) writing WiFi SSID to NVS", esp_err_to_name(err));
                    }
                }
            }
            token = strtok(NULL, "&");
        }
    }

    /* Close NVS */
    err = nvs_commit(nvsHandle);
    if ( err != ESP_OK ) {
        ESP_LOGI(TAG, "Error (%s) commiting changes to NVS", esp_err_to_name(err));
    }
    nvs_close(nvsHandle);

    // Send response
    httpd_resp_send(req, "SSID/Password change will take effect on next reboot", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

/* Handler for update POST action */
char bigbuf[4096];
static esp_err_t update_post_handler(httpd_req_t *req)
{
    int ret, remaining = req->content_len;
    esp_err_t err;

    /* Start OTA process */
    err = ota_init();
    if ( err != ESP_OK ) {
        flush_post_data(req);
        return err;
    }

    // Read any posted data
    while (remaining > 0) {
        /* Read the data for the request */
        if ((ret = httpd_req_recv(req, bigbuf,
                        MIN(remaining, sizeof(bigbuf)))) <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                /* Retry receiving if timeout occurred */
                continue;
            }
            return ota_finish( ESP_FAIL );
        }
        remaining -= ret;

        err = ota_write( bigbuf, ret );
        if ( err != ESP_OK ) {
            flush_post_data(req);
            return err;
        }
    }

    return ota_finish( ESP_OK );
}

/* Handler to respond to wildcard URI and direct the reponse */
static esp_err_t post_handler(httpd_req_t *req)
{
    /* Return one of a limited number of supported paths */
    ESP_LOGI(TAG, "POST: %s", req->uri);
    if (strncmp(req->uri, "/ctrl?", 6) == 0) {
        return ctrl_post_handler(req);
    }
    else if (strcmp(req->uri, "/config") == 0) {
        return config_post_handler(req);
    }
    else if (strcmp(req->uri, "/update") == 0) {
        return update_post_handler(req);
    }

    // Clean up any garbage
    if (flush_post_data(req) != ESP_OK)
        return ESP_FAIL;

    /* Respond with 404 Not Found */
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File does not exist");
    return ESP_FAIL;
}

static const httpd_uri_t uri_post = {
    .uri       = "/*",
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
