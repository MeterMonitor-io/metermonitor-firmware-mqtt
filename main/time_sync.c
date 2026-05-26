#include "time_sync.h"
#include "esp_sntp.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include <time.h>

static const char *TAG = "sntp";

#define TIME_SYNCED_BIT BIT0
static EventGroupHandle_t s_time_events;

static void on_time_synced(struct timeval *tv) {
    ESP_LOGI(TAG, "time synchronized");
    xEventGroupSetBits(s_time_events, TIME_SYNCED_BIT);
}

void time_sync_start(void) {
    s_time_events = xEventGroupCreate();
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_set_sync_interval(3600 * 1000);   // re-sync every hour
    esp_sntp_set_time_sync_notification_cb(on_time_synced);
    esp_sntp_init();
}

void time_sync_wait_for_sync(void) {
    xEventGroupWaitBits(s_time_events, TIME_SYNCED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
}

void time_sync_get_iso8601(char *buf, size_t len) {
    time_t now;
    struct tm info;
    time(&now);
    localtime_r(&now, &info);
    strftime(buf, len, "%Y-%m-%dT%H:%M:%S", &info);
}
