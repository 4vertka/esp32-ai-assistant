#include "freertos/projdefs.h"
#include "hal/gpio_types.h"
#include <stdio.h>

#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const gpio_num_t led = GPIO_NUM_2;

void app_main(void)
{

    gpio_reset_pin(led);
    gpio_set_direction(led, GPIO_MODE_OUTPUT);

    while (1) {
        gpio_set_level(led, 1);
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        gpio_set_level(led, 0);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

}
