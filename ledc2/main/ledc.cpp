/* LEDC

   Copywrite Brian Bulkowski, 2020

   This function controls the LEDs. It will have outboard
   functions for changing different patterns and/or artifacts.

   It will either set up its own timer, or it will have its
   own thread to blast the LEDs itself.

   It will have the definitions for the pins and whatnot - possibly in an H file 

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_spi_flash.h"

#include "driver/rmt.h"
#include "led_strip.h"


#include "esp_log.h"
static const char *TAG = "ledc2";



#define NUM_LEDS 40
#define DATA_PIN 13 
#define BRIGHTNESS  80
#define LED_TYPE    WS2811
#define COLOR_ORDER RGB

#define RMT_TX_CHANNEL RMT_CHANNEL_0

#define EXAMPLE_CHASE_SPEED_MS (30)



/**
 * @brief Simple helper function, converting HSV color space to RGB color space
 *
 * Wiki: https://en.wikipedia.org/wiki/HSL_and_HSV
 *
 */
void led_strip_hsv2rgb(uint32_t h, uint32_t s, uint32_t v, uint32_t *r, uint32_t *g, uint32_t *b)
{
    h %= 360; // h -> [0,360]
    uint32_t rgb_max = v * 2.55f;
    uint32_t rgb_min = rgb_max * (100 - s) / 100.0f;

    uint32_t i = h / 60;
    uint32_t diff = h % 60;

    // RGB adjustment amount by hue
    uint32_t rgb_adj = (rgb_max - rgb_min) * diff / 60;

    switch (i) {
    case 0:
        *r = rgb_max;
        *g = rgb_min + rgb_adj;
        *b = rgb_min;
        break;
    case 1:
        *r = rgb_max - rgb_adj;
        *g = rgb_max;
        *b = rgb_min;
        break;
    case 2:
        *r = rgb_min;
        *g = rgb_max;
        *b = rgb_min + rgb_adj;
        break;
    case 3:
        *r = rgb_min;
        *g = rgb_max - rgb_adj;
        *b = rgb_max;
        break;
    case 4:
        *r = rgb_min + rgb_adj;
        *g = rgb_min;
        *b = rgb_max;
        break;
    default:
        *r = rgb_max;
        *g = rgb_min;
        *b = rgb_max - rgb_adj;
        break;
    }
}


// going to do this the dumb way with a globals
led_strip_t     *g_strip = 0;


// Going to use the ESP timer system to attempt to get a frame rate.
// According to the documentation, this is a fairly high priority,
// and one should attempt to do minimal work - such as dispatching a message to a queue.
// at first, let's try just blasting pixels on it.

// Target frames per second
#define FASTFADE_FPS 30


static void ledc_blinkLeds(void *pvParameters){

  led_strip_t   *strip = g_strip;

    ESP_LOGI(TAG, "HSV fade start");
    uint32_t hue = 0;

    while (true) {
        hue += 1;

        uint32_t r, g, b;

        led_strip_hsv2rgb(hue, 100, 100, &r, &g, &b);

        for (int i = 0; i < NUM_LEDS; i++)
        {
            // Write RGB values to strip driver
            ESP_ERROR_CHECK(strip->set_pixel(strip, i, r, g, b));

        }

        // Flush RGB values to LEDs
        ESP_ERROR_CHECK(strip->refresh(strip, 100));

        // wait
        vTaskDelay(EXAMPLE_CHASE_SPEED_MS / portTICK_PERIOD_MS);
    }
}

// TODO: errors if errors

esp_err_t ledc_init(void) {

  ESP_LOGI(TAG," entering ledc init, call add leds");
  // the WS2811 family uses the RMT driver

    rmt_config_t config = RMT_DEFAULT_CONFIG_TX( (gpio_num_t) DATA_PIN, RMT_TX_CHANNEL);
    // set counter clock to 40MHz
    config.clk_div = 2;

    ESP_ERROR_CHECK(rmt_config(&config));
    ESP_ERROR_CHECK(rmt_driver_install(config.channel, 0, 0));

    // install ws2812 driver
    led_strip_config_t strip_config = LED_STRIP_DEFAULT_CONFIG(NUM_LEDS, (led_strip_dev_t)config.channel);
    led_strip_t *strip = led_strip_new_rmt_ws2812(&strip_config);
    if (!strip) {
        ESP_LOGE(TAG, "install WS2812 driver failed");
        return(ESP_FAIL);
    }
    // Clear LED strip (turn off all LEDs)
    ESP_ERROR_CHECK(strip->clear(strip, 100));

    // stash the strip, just annoying to pass params.
    g_strip = strip;

  // change the task below to one of the functions above to try different patterns
  ESP_LOGI(TAG,"create task for led action");

//  xTaskCreatePinnedToCore(&blinkLeds_simple, "blinkLeds", 4000/*stacksize*/, NULL/*pvparam*/, 5/*pri*/, NULL/*taskhandle*/, tskNO_AFFINITY/*coreid*/);

  xTaskCreatePinnedToCore(&ledc_blinkLeds, "blinkLeds", 4000/*stacksize*/, NULL/*pvparam*/, 5/*pri*/, NULL/*taskhandle*/, tskNO_AFFINITY/*coreid*/);

  return(ESP_OK);
}
