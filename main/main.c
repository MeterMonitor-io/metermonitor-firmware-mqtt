#include "nvs_flash.h"
#include "esp_log.h"
#include "config.h"
#include "wifi.h"
#include "time_sync.h"
#include "camera.h"
#include "mqtt.h"

static const char *TAG = "app";

void app_main(void) {
    // Init NVS — erase if layout changed (new firmware version)
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition truncated, erasing");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    config_load();

    wifi_connect(g_config.wifi_ssid, g_config.wifi_pass);
    time_sync_start();

    ESP_ERROR_CHECK(camera_init());

    mqtt_start();
    // mqtt_start() launches the capture loop task; app_main returns here.
}
