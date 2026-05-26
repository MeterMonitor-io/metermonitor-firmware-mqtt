#include "config.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "config";

mm_config_t g_config = {0};

static void nvs_read_str(nvs_handle_t h, const char *key, char *dst, size_t size, const char *def) {
    esp_err_t err = nvs_get_str(h, key, dst, &size);
    if (err != ESP_OK) {
        strncpy(dst, def, size - 1);
        dst[size - 1] = '\0';
        ESP_LOGW(TAG, "'%s' not in NVS, using default", key);
    }
}

void config_load(void) {
    nvs_handle_t h;
    esp_err_t err = nvs_open(MM_NVS_NAMESPACE, NVS_READONLY, &h);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open failed (%s) — all defaults used", esp_err_to_name(err));
        strncpy(g_config.mqtt_url,   "mqtt://192.168.1.1:1883", MQTT_URL_MAX - 1);
        strncpy(g_config.mqtt_topic, "MeterMonitor/meter",      MQTT_TOPIC_MAX - 1);
        strncpy(g_config.meter_name, "meter",                   METER_NAME_MAX - 1);
        g_config.interval = 30;
        g_config.flash_en = 1;
        return;
    }

    nvs_read_str(h, "wifi_ssid",  g_config.wifi_ssid,  WIFI_SSID_MAX,  "");
    nvs_read_str(h, "wifi_pass",  g_config.wifi_pass,  WIFI_PASS_MAX,  "");
    nvs_read_str(h, "mqtt_url",   g_config.mqtt_url,   MQTT_URL_MAX,   "mqtt://192.168.1.1:1883");
    nvs_read_str(h, "mqtt_user",  g_config.mqtt_user,  MQTT_USER_MAX,  "");
    nvs_read_str(h, "mqtt_pass",  g_config.mqtt_pass,  MQTT_PASS_MAX,  "");
    nvs_read_str(h, "mqtt_topic", g_config.mqtt_topic, MQTT_TOPIC_MAX, "MeterMonitor/meter");
    nvs_read_str(h, "meter_name", g_config.meter_name, METER_NAME_MAX, "meter");

    if (nvs_get_u32(h, "interval",      &g_config.interval)      != ESP_OK) g_config.interval      = 30;
    if (nvs_get_u8(h,  "flash_en",      &g_config.flash_en)      != ESP_OK) g_config.flash_en      = 1;
    if (nvs_get_u32(h, "flash_delay_ms", &g_config.flash_delay_ms) != ESP_OK) g_config.flash_delay_ms = 100;

    nvs_close(h);

    ESP_LOGI(TAG, "loaded: ssid=%s topic=%s interval=%lus flash=%d flash_delay=%lums",
             g_config.wifi_ssid, g_config.mqtt_topic, (unsigned long)g_config.interval,
             g_config.flash_en, (unsigned long)g_config.flash_delay_ms);
}

void config_save_interval(uint32_t interval) {
    nvs_handle_t h;
    if (nvs_open(MM_NVS_NAMESPACE, NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_u32(h, "interval", interval);
        nvs_commit(h);
        nvs_close(h);
        g_config.interval = interval;
    }
}

void config_save_flash(uint8_t flash_en) {
    nvs_handle_t h;
    if (nvs_open(MM_NVS_NAMESPACE, NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_u8(h, "flash_en", flash_en);
        nvs_commit(h);
        nvs_close(h);
        g_config.flash_en = flash_en;
    }
}

void config_save_flash_delay(uint32_t ms) {
    nvs_handle_t h;
    if (nvs_open(MM_NVS_NAMESPACE, NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_u32(h, "flash_delay_ms", ms);
        nvs_commit(h);
        nvs_close(h);
        g_config.flash_delay_ms = ms;
    }
}
