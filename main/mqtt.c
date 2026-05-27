#include "mqtt.h"
#include "config.h"
#include "camera.h"
#include "time_sync.h"
#include "mqtt_client.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "cJSON.h"
#include "mbedtls/base64.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static const char *TAG = "mqtt";

static esp_mqtt_client_handle_t s_client;
static EventGroupHandle_t       s_events;
#define MQTT_CONNECTED_BIT BIT0

static uint32_t s_picture_number = 0;

/* ── image publish ──────────────────────────────────────────────── */

static void publish_image(void) {
    camera_fb_t *fb = camera_capture();
    if (!fb) return;

    // base64-encode the JPEG buffer
    size_t b64_len = 0;
    mbedtls_base64_encode(NULL, 0, &b64_len, fb->buf, fb->len);
    char *b64 = malloc(b64_len + 1);
    if (!b64) {
        esp_camera_fb_return(fb);
        ESP_LOGE(TAG, "malloc failed for base64 buffer");
        return;
    }
    mbedtls_base64_encode((unsigned char *)b64, b64_len, &b64_len,
                          fb->buf, fb->len);
    b64[b64_len] = '\0';

    char timestamp[32];
    time_sync_get_iso8601(timestamp, sizeof(timestamp));

    int8_t rssi = 0;
    wifi_ap_record_t ap;
    if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) rssi = ap.rssi;

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "name",           g_config.meter_name);
    cJSON_AddNumberToObject(root, "picture_number", (double)(++s_picture_number));
    cJSON_AddNumberToObject(root, "WiFi-RSSI",      rssi);
    cJSON_AddNumberToObject(root, "interval",       (double)g_config.interval);

    cJSON *caps = cJSON_AddArrayToObject(root, "capabilities");
    cJSON_AddItemToArray(caps, cJSON_CreateString("capture"));
    cJSON_AddItemToArray(caps, cJSON_CreateString("flash"));
    cJSON_AddItemToArray(caps, cJSON_CreateString("flash_delay"));
    cJSON_AddItemToArray(caps, cJSON_CreateString("interval"));

    cJSON *pic = cJSON_AddObjectToObject(root, "picture");
    cJSON_AddStringToObject(pic, "format",    "jpeg");
    cJSON_AddStringToObject(pic, "timestamp", timestamp);
    cJSON_AddNumberToObject(pic, "width",  (double)fb->width);
    cJSON_AddNumberToObject(pic, "height", (double)fb->height);
    cJSON_AddNumberToObject(pic, "length", (double)fb->len);
    cJSON_AddStringToObject(pic, "data",   b64);

    esp_camera_fb_return(fb);
    free(b64);

    char *payload = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (payload) {
        int rc = esp_mqtt_client_publish(s_client, g_config.mqtt_topic,
                                         payload, strlen(payload), 0, 0);
        if (rc < 0) {
            ESP_LOGW(TAG, "publish failed (rc=%d)", rc);
        }
        free(payload);
    }
}

/* ── command handling ───────────────────────────────────────────── */

static void handle_cmd(const char *topic, const char *data, int data_len) {
    char val[32] = {0};
    int  vlen    = data_len < (int)sizeof(val) - 1 ? data_len : (int)sizeof(val) - 1;
    memcpy(val, data, vlen);

    // derive expected command topic base from the configured publish topic
    // e.g. "MeterMonitor/my_meter" → "MeterMonitor/my_meter/cmd/..."
    char base[MQTT_TOPIC_MAX + 16];
    snprintf(base, sizeof(base), "%s/cmd/", g_config.mqtt_topic);
    size_t base_len = strlen(base);

    if (strncmp(topic, base, base_len) != 0) return;
    const char *cmd = topic + base_len;

    if (strcmp(cmd, "capture") == 0) {
        ESP_LOGI(TAG, "cmd: capture");
        publish_image();

    } else if (strcmp(cmd, "flash") == 0) {
        uint8_t v = atoi(val) ? 1 : 0;
        config_save_flash(v);
        ESP_LOGI(TAG, "cmd: flash=%d", v);

    } else if (strcmp(cmd, "flash_delay") == 0) {
        uint32_t ms = (uint32_t)atoi(val);
        config_save_flash_delay(ms);
        ESP_LOGI(TAG, "cmd: flash_delay=%lums", (unsigned long)ms);

    } else if (strcmp(cmd, "interval") == 0) {
        uint32_t secs = (uint32_t)atoi(val);
        if (secs > 0) {
            config_save_interval(secs);
            ESP_LOGI(TAG, "cmd: interval=%lus", (unsigned long)secs);
        }
    }
}

/* ── capture loop task ──────────────────────────────────────────── */

static void capture_task(void *arg) {
    xEventGroupWaitBits(s_events, MQTT_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
    time_sync_wait_for_sync();
    ESP_LOGI(TAG, "capture loop started, interval=%lus", (unsigned long)g_config.interval);

    while (1) {
        publish_image();
        // Use ticks so interval changes take effect on next iteration
        vTaskDelay(pdMS_TO_TICKS((uint32_t)g_config.interval * 1000));
    }
}

/* ── MQTT event handler ─────────────────────────────────────────── */

static void mqtt_event_handler(void *arg, esp_event_base_t base,
                                int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t e = event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED: {
            ESP_LOGI(TAG, "connected to broker");
            // subscribe to all commands for this device
            char sub_topic[MQTT_TOPIC_MAX + 16];
            snprintf(sub_topic, sizeof(sub_topic), "%s/cmd/#", g_config.mqtt_topic);
            esp_mqtt_client_subscribe(e->client, sub_topic, 0);
            xEventGroupSetBits(s_events, MQTT_CONNECTED_BIT);
            break;
        }
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "disconnected — will reconnect automatically");
            xEventGroupClearBits(s_events, MQTT_CONNECTED_BIT);
            break;

        case MQTT_EVENT_DATA: {
            // topic and data are NOT null-terminated in the event struct
            char topic[MQTT_TOPIC_MAX] = {0};
            int  tlen = e->topic_len < (int)sizeof(topic) - 1
                        ? e->topic_len : (int)sizeof(topic) - 1;
            memcpy(topic, e->topic, tlen);
            handle_cmd(topic, e->data, e->data_len);
            break;
        }
        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT_EVENT_ERROR");
            break;
        default:
            break;
    }
}

/* ── public API ─────────────────────────────────────────────────── */

void mqtt_start(void) {
    s_events = xEventGroupCreate();

    esp_mqtt_client_config_t cfg = {
        .broker.address.uri = g_config.mqtt_url,
        .buffer.size        = 8192,
        .buffer.out_size    = 131072,   // 128 KB for base64-encoded images
    };

    if (g_config.mqtt_user[0]) {
        cfg.credentials.username = g_config.mqtt_user;
        cfg.credentials.authentication.password = g_config.mqtt_pass;
    }

    s_client = esp_mqtt_client_init(&cfg);
    esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(s_client);

    xTaskCreate(capture_task, "capture", 8192, NULL, 5, NULL);
}
