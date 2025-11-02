#ifndef __wifi_manager_h
#define __wifi_manager_h

#include "esp_err.h"
#include "esp_wifi.h"

#ifdef __cplusplus
extern "C" {
#endif

enum wifi_manager_status {
    WIFI_MANAGER_DISCONNECTED,
    WIFI_MANAGER_CONNECTED,
    WIFI_MANAGER_DISCONNECTING,
    WIFI_MANAGER_RECONNECTING,
    WIFI_MANAGER_AP_ACTIVE,
    WIFI_MANAGER_AP_STARTING,
    WIFI_MANAGER_AP_STOPPING,
};

typedef enum wifi_manager_status wifi_manager_status_t;

void wifi_manager_init(void);
void wifi_manager_deinit(void);
esp_err_t wifi_manager_connect_to_ap(const char* ssid, const char* key);
void wifi_manager_disconnect(void);
wifi_manager_status_t wifi_manager_get_status(void);
esp_err_t wifi_manager_scan(wifi_ap_record_t* ap_info, uint16_t* count);
const char* wifi_manager_get_local_ip(void);
int8_t wifi_manager_get_rssi(void);

// Access Point mode functions
esp_err_t wifi_manager_start_ap(const char* ssid, const char* password, uint8_t channel, uint8_t max_connections);
esp_err_t wifi_manager_stop_ap(void);
uint8_t wifi_manager_get_connected_stations(void);
bool wifi_manager_is_ap_mode(void);

#ifdef __cplusplus
}
#endif

#endif
