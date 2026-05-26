#pragma once
#include "esp_camera.h"
#include "esp_err.h"

esp_err_t    camera_init(void);
camera_fb_t *camera_capture(void);
