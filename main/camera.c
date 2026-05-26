#include "camera.h"
#include "config.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#if CONFIG_BOARD_AI_THINKER
#include "boards/ai_thinker.h"
#elif CONFIG_BOARD_M5CAM_A
#include "boards/m5cam_a.h"
#elif CONFIG_BOARD_M5CAM_B
#include "boards/m5cam_b.h"
#elif CONFIG_BOARD_ESP_EYE
#include "boards/esp_eye.h"
#elif CONFIG_BOARD_ESP32S3_EYE
#include "boards/esp32s3_eye.h"
#else
#error "No board selected — set CONFIG_BOARD_* in sdkconfig"
#endif

static const char *TAG = "camera";

esp_err_t camera_init(void) {
#if CAM_PIN_FLASH >= 0
    gpio_config_t flash_cfg = {
        .pin_bit_mask = (1ULL << CAM_PIN_FLASH),
        .mode         = GPIO_MODE_OUTPUT,
    };
    gpio_config(&flash_cfg);
    gpio_set_level(CAM_PIN_FLASH, 0);
#endif

    camera_config_t config = {
        .pin_pwdn     = CAM_PIN_PWDN,
        .pin_reset    = CAM_PIN_RESET,
        .pin_xclk     = CAM_PIN_XCLK,
        .pin_sccb_sda = CAM_PIN_SIOD,
        .pin_sccb_scl = CAM_PIN_SIOC,
        .pin_d7       = CAM_PIN_D7,
        .pin_d6       = CAM_PIN_D6,
        .pin_d5       = CAM_PIN_D5,
        .pin_d4       = CAM_PIN_D4,
        .pin_d3       = CAM_PIN_D3,
        .pin_d2       = CAM_PIN_D2,
        .pin_d1       = CAM_PIN_D1,
        .pin_d0       = CAM_PIN_D0,
        .pin_vsync    = CAM_PIN_VSYNC,
        .pin_href     = CAM_PIN_HREF,
        .pin_pclk     = CAM_PIN_PCLK,
        .xclk_freq_hz = 20000000,
        .ledc_timer   = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,
        .pixel_format = PIXFORMAT_JPEG,
        .frame_size   = FRAMESIZE_VGA,
        .jpeg_quality = 12,
        .fb_count     = 1,
        .grab_mode    = CAMERA_GRAB_WHEN_EMPTY,
    };

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_camera_init failed: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "camera ready");
    }
    return err;
}

camera_fb_t *camera_capture(void) {
#if CAM_PIN_FLASH >= 0
    if (g_config.flash_en) {
        gpio_set_level(CAM_PIN_FLASH, 1);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
#endif

    camera_fb_t *fb = esp_camera_fb_get();

#if CAM_PIN_FLASH >= 0
    gpio_set_level(CAM_PIN_FLASH, 0);
#endif

    if (!fb) {
        ESP_LOGE(TAG, "esp_camera_fb_get failed");
    }
    return fb;
}
