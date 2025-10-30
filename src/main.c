//#include "accessory_driver.h"
#include "api/broadcast.h"
//#include "bluetooth_driver.h"
#include "config.h"
#include "config_defs.h"
//#include "console.h"
#include "eom-hal-esp32.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
//#include "bluetooth_gatt_server.h"
#include "gatt_svc.h"
#include "orgasm_control.h"
#include "polyfill.h"
//#include "system/action_manager.h"
#include "system/http_server.h"
////#include "ui/toast.h"
//#include "ui/ui.h"
//#include "util/i18n.h"
#include "version.h"
#include "wifi_manager.h"
#include <esp_https_ota.h>
#include <esp_ota_ops.h>
#include <esp_system.h>
#include <time.h>
#include <nvs_flash.h>
#include <esp_spiffs.h>


char* TAG = "main";


void spiffs_init() {
        ESP_LOGI(TAG, "Initializing SPIFFS");

    esp_vfs_spiffs_conf_t conf = {
      .base_path = "/spiffs",
      .partition_label = "spiffs",
      .max_files = 5,
      .format_if_mount_failed = true
    };

    // Use settings defined above to initialize and mount SPIFFS filesystem.
    // Note: esp_vfs_spiffs_register is an all-in-one convenience function.
    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return;
    }
    ESP_LOGI(TAG, "SPIFFS registered");


    // ESP_LOGI(TAG, "Performing SPIFFS_check().");
    // ret = esp_spiffs_check(conf.partition_label);
    // if (ret != ESP_OK) {
    //     ESP_LOGE(TAG, "SPIFFS_check() failed (%s)", esp_err_to_name(ret));
    //     return;
    // } else {
    //     ESP_LOGI(TAG, "SPIFFS_check() successful");
    // }

    // size_t total = 0, used = 0;
    // ret = esp_spiffs_info(conf.partition_label, &total, &used);
    // if (ret != ESP_OK) {
    //     ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s). Formatting...", esp_err_to_name(ret));
    //     esp_spiffs_format(conf.partition_label);
    //     return;
    // } else {
    //     ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    // }

    // // Check consistency of reported partition size info.
    // if (used > total) {
    //     ESP_LOGW(TAG, "Number of used bytes cannot be larger than total. Performing SPIFFS_check().");
    //     ret = esp_spiffs_check(conf.partition_label);
    //     // Could be also used to mend broken files, to clean unreferenced pages, etc.
    //     // More info at https://github.com/pellepl/spiffs/wiki/FAQ#powerlosses-contd-when-should-i-run-spiffs_check
    //     if (ret != ESP_OK) {
    //         ESP_LOGE(TAG, "SPIFFS_check() failed (%s)", esp_err_to_name(ret));
    //         return;
    //     } else {
    //         ESP_LOGI(TAG, "SPIFFS_check() successful");
    //     }
    // }

    // // Use POSIX and C standard library functions to work with files.
    // // First create a file.
    // ESP_LOGI(TAG, "Opening file");
    // FILE* f = fopen("/spiffs/hello.txt", "w");
    // if (f == NULL) {
    //     ESP_LOGE(TAG, "Failed to open file for writing");
    //     return;
    // }
    // fprintf(f, "Hello World!\n");
    // fclose(f);
    // ESP_LOGI(TAG, "File written");

    // // Check if destination file exists before renaming
    // struct stat st;
    // if (stat("/spiffs/foo.txt", &st) == 0) {
    //     // Delete it if it exists
    //     unlink("/spiffs/foo.txt");
    // }

    // // Rename original file
    // ESP_LOGI(TAG, "Renaming file");
    // if (rename("/spiffs/hello.txt", "/spiffs/foo.txt") != 0) {
    //     ESP_LOGE(TAG, "Rename failed");
    //     return;
    // }

    // // Open renamed file for reading
    // ESP_LOGI(TAG, "Reading file");
    // f = fopen("/spiffs/foo.txt", "r");
    // if (f == NULL) {
    //     ESP_LOGE(TAG, "Failed to open file for reading");
    //     return;
    // }
    // // char line[64];
    // // fgets(line, sizeof(line), f);
    // fclose(f);
    // // strip newline
    // char* pos = strchr(line, '\n');
    // if (pos) {
    //     *pos = '\0';
    // }
    // ESP_LOGI(TAG, "Read from file: '%s'", line);
}

static void orgasm_task(void* args) {
    // for (;;) {
    orgasm_control_tick();

    // vTaskDelay(1);
    // }
}

static void ble_task(void* args) {
    // for (;;) {   
    send_output1();

    vTaskDelay(1);
    // }
}

static void hal_task(void* args) {
    // for (;;) {
    //eom_hal_tick();

     vTaskDelay(1);
    // }
}


static void loop_task(void* args) {
    // for (;;) {
    static long lastStatusTick = 0;
    static long lastTick = 0;

    // Periodically send out WiFi status:
    if (millis() - lastStatusTick > 1000 * 10) {
        lastStatusTick = millis();
        api_broadcast_network_status();

        // Update Icons
        if (wifi_manager_get_status() == WIFI_MANAGER_CONNECTED) {
            int8_t rssi = wifi_manager_get_rssi();
            //ui_set_icon(UI_ICON_WIFI, WIFI_ICON_STRONG_SIGNAL);
        } else {
            //ui_set_icon(UI_ICON_WIFI, WIFI_ICON_DISCONNECTED);
        }
    }

    if (millis() - lastTick > 1000 / 15) {
        ESP_LOGD(
            TAG,
            "%%heap=%d, min=%d, internal=%d",
            esp_get_free_heap_size(),
            esp_get_minimum_free_heap_size(),
            esp_get_free_internal_heap_size()
        );

        lastTick = millis();
        api_broadcast_readings();
    }

    // Tick and see if we need to save config:
    config_enqueue_save(-1);

    vTaskDelay(1);
    // }
}

// static void accessory_driver_task(void* args) {
//     while (true) {
//         bluetooth_driver_tick();
//         vTaskDelay(1);
//     }
// }

static void main_task(void* args) {
    //console_ready();
    //ui_open_page(&PAGE_EDGING_STATS, NULL);
    //ui_reset_idle_timer();
    orgasm_control_set_output_mode(OC_AUTOMATIC);

    for (;;) {
        loop_task(NULL);
        //hal_task(NULL);
        //ui_task(NULL);
        orgasm_task(NULL);
        ble_task(NULL);
    }
}

esp_err_t run_boot_diagnostic(void) {
    esp_err_t err = ESP_ERR_INVALID_ARG;
    esp_ota_img_states_t ota_state = ESP_OTA_IMG_UNDEFINED;

    const esp_partition_t* configured = esp_ota_get_boot_partition();
    const esp_partition_t* running = esp_ota_get_running_partition();

    if (configured != running) {
        ESP_LOGW(
            TAG,
            "Configured OTA boot partition at offset 0x%08" PRIx32
            ", but running from offset 0x%08" PRIx32,
            configured->address,
            running->address
        );
        ESP_LOGW(
            TAG,
            "(This can happen if either the OTA boot data or preferred boot image become corrupted "
            "somehow.)"
        );
    }

    ESP_LOGD(
        TAG,
        "Running partition type %d subtype %d (offset 0x%08" PRIx32 ")",
        running->type,
        running->subtype,
        running->address
    );

    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
            // if (diagnostic_is_ok) {
            ESP_LOGI(TAG, "Diagnostics completed successfully! Continuing execution ...");
            esp_ota_mark_app_valid_cancel_rollback();
            err = ESP_OK;
            // } else {
            // ESP_LOGE(TAG, "Diagnostics failed! Start rollback to the previous version ...");
            // esp_ota_mark_app_invalid_rollback_and_reboot();
            // }
        }
    }

    return err;
}

void app_main() {
    esp_log_level_set(TAG, ESP_LOG_DEBUG);
    TickType_t boot_tick = xTaskGetTickCount();

    //eom_hal_init();

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    //eom_hal_setup_pressure_ambient();

    spiffs_init();
    config_init();
    //console_init();
    //accessory_driver_init();
    orgasm_control_init();
    eom_hal_init_motor();
    //i18n_init();
    //action_manager_init();


    // Welcome Preamble
    printf("Lamystere presents the Endless Orgasm Machine\n");
    printf("Version: %s\n", EOM_VERSION);
    //printf("EOM-HAL Version: %s\n", eom_hal_get_version());

    //Post-Update Diagnostics
    esp_err_t dxerr = run_boot_diagnostic();
    if (dxerr != ESP_ERR_INVALID_ARG) {
        if (dxerr == ESP_OK) {
            //ui_toast("%s\n%s", "Update complete.", EOM_VERSION);
        }
    }

    // Go to the splash page:
    //ui_open_page(&SPLASH_PAGE, NULL);

    // Green = prepare Networking
    vTaskDelayUntil(&boot_tick, 1000UL / portTICK_PERIOD_MS);
    //eom_hal_set_encoder_rgb(0, 255, 0);

    // Initialize WiFi
    if (Config.wifi_on) {
        http_server_init();
        wifi_manager_init();

        if (ESP_OK == wifi_manager_connect_to_ap(Config.wifi_ssid, Config.wifi_key)) {
            //ui_set_icon(UI_ICON_WIFI, WIFI_ICON_WEAK_SIGNAL);
        } else {
            //ui_set_icon(UI_ICON_WIFI, WIFI_ICON_DISCONNECTED);
        }
    }

    // Blue = prepare Bluetooth and Drivers
    vTaskDelayUntil(&boot_tick, 1000UL / portTICK_PERIOD_MS);
    //eom_hal_set_encoder_rgb(0, 0, 255);

    // Initialize Bluetooth
    if (Config.bt_on) {
        ble_host_config_init();
        //bluetooth_gatt_server_init();
        //ui_set_icon(UI_ICON_BT, BT_ICON_ACTIVE);
    } else {
        //ui_set_icon(UI_ICON_BT, -1);
    }

    // Initialize Action Manager
    //action_manager_load_all_plugins();

    // Final delay on encoder colors.
    vTaskDelayUntil(&boot_tick, 1000UL / portTICK_PERIOD_MS);
    //ui_fade_to(0x00);

    ESP_LOGI(TAG, "System initialization complete.");
    ESP_LOGI(TAG, "IP Address: %s", wifi_manager_get_local_ip());
    //xTaskCreate(accessory_driver_task, "ACCESSORY", 1024 * 4, NULL, tskIDLE_PRIORITY, NULL);
    xTaskCreate(main_task, "MAIN", 1024 * 12, NULL, tskIDLE_PRIORITY + 1, NULL);
}