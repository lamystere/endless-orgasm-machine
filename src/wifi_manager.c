#include "wifi_manager.h"
#include "config.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_tls.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "mdns.h"
#include "nvs_flash.h"
#include "esp_sntp.h"
#include "system/http_server.h"
#include <stdbool.h>
#include <string.h>

static const char* TAG = "wifi_manager";

/* Some SDK versions may not expose MAC2STR; provide a local fallback */
#ifndef MAC2STR
#define MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
#endif

static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;
static char s_wifi_ip_addr_str[20] = "";
static bool s_initialized = false;
static wifi_manager_status_t s_wifi_status = WIFI_MANAGER_DISCONNECTED;
static uint8_t s_connected_stations = 0;
static esp_netif_t* s_netif_ap = NULL;


#define WIFI_MAX_CONNECTION_RETRY 5

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1
#define WIFI_AP_STARTED_BIT BIT2

static void
event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT) {
        if (event_id == WIFI_EVENT_STA_START) {
            wifi_config_t config = { 0 };
            esp_wifi_get_config(WIFI_IF_STA, &config);

            if (config.ap.ssid[0] != '\0'){ //} && Config.wifi_on) {
                ESP_LOGI(TAG, "Auto-connect WiFi to: %s", config.ap.ssid);
                esp_wifi_connect();
            }
        } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
            if (s_wifi_status == WIFI_MANAGER_DISCONNECTING) {
                s_wifi_status = WIFI_MANAGER_DISCONNECTED;
                return;
            } else if (s_retry_num < WIFI_MAX_CONNECTION_RETRY) {
                s_wifi_status = WIFI_MANAGER_RECONNECTING;
                esp_wifi_connect();
                s_retry_num++;
                ESP_LOGI(TAG, "WiFi Disconnected, Retrying (attempt %d)", s_retry_num);
            } else {
                s_wifi_status = WIFI_MANAGER_DISCONNECTED;
                xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
                ESP_LOGW(TAG, "WiFi Disconnected, exceeded retry limit.");
            }
        } else if (event_id == WIFI_EVENT_AP_START) {
            ESP_LOGI(TAG, "Access Point started");
            s_wifi_status = WIFI_MANAGER_AP_ACTIVE;
            xEventGroupSetBits(s_wifi_event_group, WIFI_AP_STARTED_BIT);
            
            // Get AP IP address
            esp_netif_ip_info_t ip_info;
            esp_netif_get_ip_info(s_netif_ap, &ip_info);
            snprintf(s_wifi_ip_addr_str, 20, IPSTR, IP2STR(&ip_info.ip));
            ESP_LOGI(TAG, "AP IP address: %s", s_wifi_ip_addr_str);
        } else if (event_id == WIFI_EVENT_AP_STOP) {
            ESP_LOGI(TAG, "Access Point stopped");
            s_wifi_status = WIFI_MANAGER_DISCONNECTED;
            s_connected_stations = 0;
        } else if (event_id == WIFI_EVENT_AP_STACONNECTED) {
            wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
            s_connected_stations++;
            ESP_LOGI(TAG, "Station %02x:%02x:%02x:%02x:%02x:%02x connected to AP, total: %d", MAC2STR(event->mac), s_connected_stations);
        } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
            wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
            if (s_connected_stations > 0) {
                s_connected_stations--;
            }
            ESP_LOGI(TAG, "Station %02x:%02x:%02x:%02x:%02x:%02x disconnected from AP, total: %d", 
                     MAC2STR(event->mac), s_connected_stations);
        }
    } else if (event_base == IP_EVENT) {
        if (event_id == IP_EVENT_STA_GOT_IP) {
            ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
            snprintf(s_wifi_ip_addr_str, 20, IPSTR, IP2STR(&event->ip_info.ip));
            ESP_LOGI(TAG, "Got IP: %s", s_wifi_ip_addr_str);
            s_retry_num = 0;
            xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
            s_wifi_status = WIFI_MANAGER_CONNECTED;
        }
    }
}

void wifi_manager_init(void) {
    if (s_initialized) return;

    s_wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_tls_init_global_ca_store());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &instance_any_id
    ));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, &instance_got_ip
    ));

    // Don't save WiFi creds.
    esp_wifi_set_storage(WIFI_STORAGE_RAM);

    // Initialize wifi based on configuration
    ESP_LOGI(TAG, "WiFi mode: %s", Config.wifi_ap_mode ? "Access Point" : "Station");
    if (Config.wifi_ap_mode) {
        // Create AP netif
        s_netif_ap = esp_netif_create_default_wifi_ap();
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
        ESP_LOGI(TAG, "WiFi initialized in Access Point mode");
    } else {
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_LOGI(TAG, "WiFi initialized in Station mode");
    }
    
    ESP_ERROR_CHECK(esp_wifi_start());

    // Use Internet Time
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_init();

    s_initialized = true;
    
    // Auto-start if configured
    if (Config.wifi_on) {
        if (Config.wifi_ap_mode) {
            ESP_LOGI(TAG, "Auto-starting Access Point from config");
            wifi_manager_start_ap(Config.ap_ssid, Config.ap_password, Config.ap_channel, Config.ap_max_connections);
        } else {
            ESP_LOGI(TAG, "Auto-connecting to AP from config");
            if (wifi_manager_connect_to_ap(Config.wifi_ssid, Config.wifi_key) != ESP_OK) {
                ESP_LOGW(TAG, "Failed to connect to configured AP. Starting standalone AP.");
                wifi_manager_start_ap(Config.ap_ssid, Config.ap_password, Config.ap_channel, Config.ap_max_connections);
            }
        }
    }
}

/** @todo Pelase actually implement me for the love of Yusuke Kitagawa. */
void wifi_manager_deinit(void) {
    wifi_manager_disconnect();
}

esp_err_t wifi_manager_connect_to_ap(const char* ssid, const char* key) {
    if (ssid[0] == '\0') {
        ESP_LOGW(TAG, "Connecting to AP requires SSID. Aborting.");
        return ESP_ERR_INVALID_ARG;
    }

    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false,
            },
        },
    };

    strlcpy((char*)wifi_config.sta.ssid, ssid, 32);
    strlcpy((char*)wifi_config.sta.password, key, 64);

    esp_wifi_stop();
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Connecting to WiFi: \"%s\"", wifi_config.sta.ssid);
    ESP_LOGI(TAG, "    Using Password: \"%s\"", wifi_config.sta.password);

    EventBits_t bits = xEventGroupWaitBits(
        s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE, portMAX_DELAY
    );

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to AP.");
        // Set mDNS hostname
        if (Config.hostname[0] != '\0') {
            ESP_ERROR_CHECK(mdns_init());
            mdns_hostname_set(Config.hostname);
            mdns_instance_name_set(Config.bt_display_name);
        }

        if (Config.websocket_port > 0) {
            ESP_ERROR_CHECK(http_server_connect());
        }

        return ESP_OK;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "Connection failure, check other logs.");
        return ESP_FAIL;
    } else {
        ESP_LOGE(TAG, "Unknown bitmask for connection.");
        return ESP_FAIL;
    }
}

/**
 * Scan for WiFi networks and populate a buffer. *count should initially contain the maximum number
 * of networks to scan. After this function returns, it will contain the number of networks found.
 */
esp_err_t wifi_manager_scan(wifi_ap_record_t* ap_info, uint16_t* count) {
    esp_err_t err = ESP_OK;
    uint16_t len = *count;
    uint16_t ap_count = 0;
    memset(ap_info, 0, sizeof(wifi_ap_record_t) * len);

    // Start a blocking scan:
    err = esp_wifi_scan_start(NULL, true);
    if (err != ESP_OK) {
        return err;
    }

    err = esp_wifi_scan_get_ap_records(count, ap_info);
    if (err != ESP_OK) {
        return err;
    }
    err = esp_wifi_scan_get_ap_num(&ap_count);
    if (err != ESP_OK) {
        return err;
    }

    if (ap_count < *count) {
        *count = ap_count;
    }
    return err;
}

void wifi_manager_disconnect(void) {
    s_wifi_status = WIFI_MANAGER_DISCONNECTING;
    http_server_disconnect();
    esp_wifi_disconnect();
}

wifi_manager_status_t wifi_manager_get_status(void) {
    return s_wifi_status;
}

int8_t wifi_manager_get_rssi(void) {
    wifi_ap_record_t ap;
    esp_wifi_sta_get_ap_info(&ap);
    return ap.rssi;
}

const char* wifi_manager_get_local_ip(void) {
    return s_wifi_ip_addr_str;
}

esp_err_t wifi_manager_start_ap(const char* ssid, const char* password, uint8_t channel, uint8_t max_connections) {
    if (!s_initialized) {
        ESP_LOGE(TAG, "WiFi manager not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (ssid == NULL || strlen(ssid) == 0) {
        ESP_LOGE(TAG, "AP SSID cannot be empty");
        return ESP_ERR_INVALID_ARG;
    }

    if (channel < 1 || channel > 13) {
        ESP_LOGW(TAG, "Invalid channel %d, using default channel 1", channel);
        channel = 1;
    }

    if (max_connections < 1 || max_connections > 4) {
        ESP_LOGW(TAG, "Invalid max_connections %d, using default 4", max_connections);
        max_connections = 4;
    }

    s_wifi_status = WIFI_MANAGER_AP_STARTING;

    // Stop current WiFi if running
    esp_wifi_stop();

    // Create AP netif if not exists
    if (s_netif_ap == NULL) {
        s_netif_ap = esp_netif_create_default_wifi_ap();
    }

    // Configure AP
    wifi_config_t wifi_config = {
        .ap = {
            .channel = channel,
            .max_connection = max_connections,
            .authmode = (password && strlen(password) > 0) ? WIFI_AUTH_WPA_WPA2_PSK : WIFI_AUTH_OPEN,
            .pmf_cfg = {
                .required = false,
            },
        },
    };

    strlcpy((char*)wifi_config.ap.ssid, ssid, sizeof(wifi_config.ap.ssid));
    wifi_config.ap.ssid_len = strlen(ssid);

    if (password && strlen(password) > 0) {
        strlcpy((char*)wifi_config.ap.password, password, sizeof(wifi_config.ap.password));
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Starting Access Point: SSID=%s, Channel=%d, Max Connections=%d", 
             ssid, channel, max_connections);

    // Wait for AP to start
    EventBits_t bits = xEventGroupWaitBits(
        s_wifi_event_group, WIFI_AP_STARTED_BIT, pdFALSE, pdFALSE, portMAX_DELAY
    );

    if (bits & WIFI_AP_STARTED_BIT) {
        ESP_LOGI(TAG, "Access Point started successfully");
        
        // Set mDNS hostname
        if (Config.hostname[0] != '\0') {
            ESP_ERROR_CHECK(mdns_init());
            mdns_hostname_set(Config.hostname);
            mdns_instance_name_set(Config.bt_display_name);
        }

        // Start HTTP server
        if (Config.websocket_port > 0) {
            ESP_ERROR_CHECK(http_server_connect());
        }

        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Failed to start Access Point");
        return ESP_FAIL;
    }
}

esp_err_t wifi_manager_stop_ap(void) {
    if (s_wifi_status != WIFI_MANAGER_AP_ACTIVE) {
        ESP_LOGW(TAG, "Access Point is not active");
        return ESP_ERR_INVALID_STATE;
    }

    s_wifi_status = WIFI_MANAGER_AP_STOPPING;
    http_server_disconnect();
    esp_wifi_stop();
    
    return ESP_OK;
}

uint8_t wifi_manager_get_connected_stations(void) {
    return s_connected_stations;
}

bool wifi_manager_is_ap_mode(void) {
    return (s_wifi_status == WIFI_MANAGER_AP_ACTIVE || 
            s_wifi_status == WIFI_MANAGER_AP_STARTING ||
            s_wifi_status == WIFI_MANAGER_AP_STOPPING);
}