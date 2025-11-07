#include "system/http_server.h"
#include "api/index.h"
#include "config.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_https_server.h"
#include "esp_log.h"
#include "esp_system.h"
#include "system/websocket_handler.h"
#include "util/fs.h"
#include "esp_wifi.h"
#include <sys/stat.h>
#include <dirent.h>
#include <stdio.h>

#define DEFAULT_CACERT_PATH "/spiffs/ui/server.crt"
#define DEFAULT_PRVTKEY_PATH "/spiffs/ui/server.key"
#define SCRATCH_BUFSIZE  16384

static const char* TAG = "http_server";
static httpd_handle_t _server = NULL;
static char* index_html = NULL;
char scratch[SCRATCH_BUFSIZE];
struct stat file_stat;

esp_err_t http_handler(httpd_req_t* req) {
    char filepath[128];
    strcpy(filepath, "/spiffs");
    
    // If filepath doesn't contain "/ui", append it
    if (strstr(req->uri, "/ui") == NULL) {
        strncat(filepath, "/ui", sizeof(filepath) - strlen(filepath) - 1);
    }
    strncat(filepath, req->uri, sizeof(filepath) - strlen(filepath) - 1);
    
    if ((filepath[strlen(filepath) - 1] == '/')) {
        strncat(filepath, "index.html", sizeof(filepath) - strlen(filepath) - 1);
    }
    if (strchr(filepath, '.') == NULL) {
        strncat(filepath, "/index.html", sizeof(filepath) - strlen(filepath) - 1);
    }
    ESP_LOGI(TAG, "Sending file : %s", filepath);
    httpd_resp_set_type(req, "text/html");

    FILE* fd = fopen(filepath, "r");
    if (!fd) {
        ESP_LOGE(TAG, "Failed to read existing file : %s", filepath);
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read existing file");
        return ESP_FAIL;
    }

    /* Use local scratch buffer for temporary storage */
    char *chunk = scratch;
    size_t chunksize;
    do {
        /* Read file in chunks into the scratch buffer */
        chunksize = fread(chunk, 1, SCRATCH_BUFSIZE, fd);

        if (chunksize > 0) {
            /* Send the buffer contents as HTTP response chunk */
            if (httpd_resp_send_chunk(req, chunk, chunksize) != ESP_OK) {
                fclose(fd);
                ESP_LOGE(TAG, "File sending failed!");
                httpd_resp_send_chunk(req, NULL, 0);
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
               return ESP_FAIL;
           }
        }
    } while (chunksize != 0);
    /* Close file after sending complete */
    fclose(fd);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

esp_err_t http_or_websocket_handler(httpd_req_t* req) {
    //havent figured out how to serve both ws and http on same route yet .. for now start with "/ui" endpoint.  "/"" endpoint needs to be ws for backwards compatibility

    //if (req->hdr_fields_len > 0) { // Check for headers to avoid crashing on empty requests
    char dummy[128];
    esp_err_t hdr_ret = httpd_req_get_hdr_value_str(req, "Upgrade", dummy, sizeof(dummy));
    char *upgrade_header = (hdr_ret == ESP_OK) ? dummy : NULL;
        if (upgrade_header != NULL && strcmp(upgrade_header, "websocket") == 0) {
            ESP_LOGI("Handler", "WebSocket upgrade request received");
            //return httpd_ws_upgrade_req_begin(req); // Initiate WebSocket handshake
            return websocket_handler(req);
        }
    //}
    // If not a WebSocket upgrade, handle as a standard HTTP GET request
    return http_handler(req);

    // if (httpd_req_get_hdr_value_len(req, "Sec-WebSocket-Key") > 0) {
    //     return websocket_handler(req);
    // } else {
    //     return http_handler(req);
    // }
}

static const httpd_uri_t routes[] = { 
    {
        .uri = "/",
        .method = HTTP_GET,
        .handler = &websocket_handler,
        .user_ctx = NULL,
        .is_websocket = true,
        .handle_ws_control_frames = true,
    } ,
    {
        .uri = "/ui",
        .method = HTTP_GET,
        .handler = &http_handler,
        .user_ctx = NULL,
        .is_websocket = false,
        .handle_ws_control_frames = false,
    } ,
    {
        .uri = "/ui/",
        .method = HTTP_GET,
        .handler = &http_handler,
        .user_ctx = NULL,
        .is_websocket = false,
        .handle_ws_control_frames = false,
    } ,
};


static void init_routes(httpd_handle_t server) {
    for (size_t i = 0; i < sizeof(routes) / sizeof(routes[0]); i++) {
        httpd_uri_t route = routes[i];
        ESP_LOGI(TAG, "* Route: %s", route.uri);
        httpd_register_uri_handler(server, &route);
    }
}

static httpd_handle_t start_webserver(void) {
    esp_err_t err = ESP_OK;
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    if (Config.websocket_port > 0) {
        config.server_port = Config.websocket_port;
    }

    config.lru_purge_enable = true;

    // Open/Close callbacks
    config.open_fn = websocket_open_fd;
    config.close_fn = websocket_close_fd;

    if (!Config.use_ssl) {
        ESP_LOGI(TAG, "Starting server on port: %d", config.server_port);
        err = httpd_start(&server, &config);
    } else {
        ESP_LOGI(
            TAG,
            "Starting SSL server on port: %d (%d bytes free)",
            config.server_port,
            esp_get_free_heap_size()
        );

        httpd_ssl_config_t ssl_config = HTTPD_SSL_CONFIG_DEFAULT();
        ssl_config.httpd = config;
        DIR *dir = opendir("/spiffs");
        if (!dir) {
            ESP_LOGE(TAG, "Failed to open /spiffs directory. Is SPIFFS mounted?");
            return NULL;
        } else {
            struct dirent *entry;
            while ((entry = readdir(dir)) != NULL) {
                ESP_LOGI(TAG, "Found file: %s", entry->d_name);
            }
        }
        closedir(dir);
        char path[CONFIG_PATH_MAX];

        sniprintf(
            path,
            CONFIG_PATH_MAX,
            "%s",
            Config.ssl_servercert_path ? Config.ssl_servercert_path : DEFAULT_CACERT_PATH
        );

        ssl_config.servercert_len = fs_read_file(path, (char**)&ssl_config.servercert);
        //servercert = ssl_config.servercert;

        if (ssl_config.servercert_len <= 0) {
            ESP_LOGE(TAG, "Error reading CA Certificate.");
            return NULL;
        } else {
            ESP_LOGD(TAG, "%d%s", ssl_config.servercert_len, ssl_config.servercert);
        }

        sniprintf(
            path,
            CONFIG_PATH_MAX,
            "%s",
            Config.ssl_prvtkey_path ? Config.ssl_prvtkey_path : DEFAULT_PRVTKEY_PATH
        );

        ssl_config.prvtkey_len = fs_read_file(path, (char**)&ssl_config.prvtkey_pem);
        //prvtkey_pem = ssl_config.prvtkey_pem;

        if (ssl_config.prvtkey_len <= 0) {
            ESP_LOGE(TAG, "Error reading SSL Private Key.");
            return NULL;
        } else {
            ESP_LOGI(TAG, "%d%s", ssl_config.prvtkey_len, ssl_config.prvtkey_pem);
        }

        ESP_LOGI(TAG, "SSL Certificates provisioned. (%d bytes free)", esp_get_free_heap_size());
        err = httpd_ssl_start(&server, &ssl_config);
        

        ESP_LOGI(TAG, "SSL Server started.");


    }

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error starting server: %s", esp_err_to_name(err));
        return NULL;
    }

    ESP_LOGI(TAG, "Server started, registering routes.");
    init_routes(server);
    
    // Start the WebSocket ping timer
    //websocket_start_ping_timer();

    return server;
}

static void stop_webserver(httpd_handle_t server) {
    //websocket_stop_ping_timer();
    httpd_stop(server);
}

static void
connect_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    httpd_handle_t* server = (httpd_handle_t*)arg;
    if (*server == NULL) {
        ESP_LOGI(TAG, "Starting webserver...");
        *server = start_webserver();
    }
}

static void
disconnect_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    httpd_handle_t* server = (httpd_handle_t*)arg;
    if (*server) {
        ESP_LOGI(TAG, "Stopping webserver...");
        stop_webserver(*server);
        *server = NULL;
    }
}

esp_err_t http_server_init(void) {
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &connect_handler, &_server);
    esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &disconnect_handler, &_server);

    api_register_all();
    return ESP_OK;
}

esp_err_t http_server_connect(void) {
    _server = start_webserver();
    if (_server == NULL) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

void http_server_disconnect(void) {
    stop_webserver(_server);
    _server = NULL;
}