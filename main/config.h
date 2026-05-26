#pragma once
#include <stdint.h>

#define MM_NVS_NAMESPACE "config"

#define WIFI_SSID_MAX    64
#define WIFI_PASS_MAX    64
#define MQTT_URL_MAX    128
#define MQTT_USER_MAX    64
#define MQTT_PASS_MAX    64
#define MQTT_TOPIC_MAX  128
#define METER_NAME_MAX   64

typedef struct {
    char     wifi_ssid[WIFI_SSID_MAX];
    char     wifi_pass[WIFI_PASS_MAX];
    char     mqtt_url[MQTT_URL_MAX];
    char     mqtt_user[MQTT_USER_MAX];
    char     mqtt_pass[MQTT_PASS_MAX];
    char     mqtt_topic[MQTT_TOPIC_MAX];
    char     meter_name[METER_NAME_MAX];
    uint32_t interval;
    uint8_t  flash_en;
    uint32_t flash_delay_ms;
} mm_config_t;

extern mm_config_t g_config;

void config_load(void);
void config_save_interval(uint32_t interval);
void config_save_flash(uint8_t flash_en);
void config_save_flash_delay(uint32_t ms);
