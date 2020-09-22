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

#include "FastLED.h"
#include "FX.h"

#include "esp_log.h"
static const char *TAG = "ledc";


CRGBPalette16 currentPalette;
TBlendType    currentBlending;

extern CRGBPalette16 myRedWhiteBluePalette;
extern const TProgmemPalette16 IRAM_ATTR myRedWhiteBluePalette_p;

#include "palettes.h"

#define NUM_LEDS 40
#define DATA_PIN 13 
#define BRIGHTNESS  80
#define LED_TYPE    WS2811
#define COLOR_ORDER RGB

CRGB leds[NUM_LEDS];



#if 0

// Going to use the ESP timer system to attempt to get a frame rate.
// According to the documentation, this is a fairly high priority,
// and one should attempt to do minimal work - such as dispatching a message to a queue.
// at first, let's try just blasting pixels on it.

// Target frames per second
#define FASTFADE_FPS 40

typedef struct {
  CHSV color;
} ledc_fastfade1_t;

static void _ledc_fastfade1_cb(void *param){

  ledc_fastfade1_t *ff = (ledc_fastfade1_t *)param;

  ff->color.hue++;

  //ESP_LOGI(TAG,"fast hsv fade h: %d s: %d v: %d",ff->color.hue,ff->color.s, ff->color.v);

  fill_solid(leds,NUM_LEDS,ff->color);

  FastLED.show();

};

static void ledc_fastfade(void *pvParameters){

  ledc_fastfade1_t ff1_t = {
    .color = CHSV(0/*hue*/,255/*sat*/,255/*value*/)
  };

  esp_timer_create_args_t timer_create_args = {
        .callback = _ledc_fastfade1_cb,
        .arg = (void *) &ff1_t,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "fastfade_timer"
    };

  esp_timer_handle_t timer_h;

  esp_timer_create(&timer_create_args, &timer_h);

  esp_timer_start_periodic(timer_h, 1000000L / FASTFADE_FPS );

  // suck- just trying this
  while(1){

      vTaskDelay(1000 / portTICK_PERIOD_MS);
  };

}

#endif /* 0 */

WS2812FX *g_ws2812fx = 0;
bool g_ws2812_mode_random = true;

esp_err_t ledc_led_mode_set(int mode) {
  ESP_LOGI(TAG,"ledc: set mode %d",mode);

  if(g_ws2812fx) {
      WS2812FX::Segment *segments = g_ws2812fx->getSegments();
      g_ws2812_mode_random = false;
      // mode has a special setter, unlike many other things
      for (uint8_t i = 0; i < MAX_NUM_SEGMENTS; i++)
      {
        g_ws2812fx->setMode(i, mode);
        segments[0].colors[0] = 0xff0000; // red for testing
      }
  }
  return(ESP_OK);
}

// will get the default mode 0
int ledc_led_mode_get(void) {
  if (!g_ws2812fx) return(-1);

  ESP_LOGI(TAG,"ledc: get mode %d",g_ws2812fx->getMode());

  return(g_ws2812fx->getMode());
}

/*
** what is speed?
** https://github.com/kitesurfer1404/WS2812FX/issues/113
** it's in milliseconds, so let's multiply / divide by 10
*/

#define SPEED_FACTOR 10

esp_err_t ledc_led_speed_set(int speed) {
  speed *= SPEED_FACTOR;
  ESP_LOGI(TAG,"ledc: set speed %d",speed);
  if(g_ws2812fx) {
    WS2812FX::Segment *segments = g_ws2812fx->getSegments();
    for (uint8_t i = 0; i < MAX_NUM_SEGMENTS; i++)
    {
      segments[i].speed = speed;
    }
  }
  return(ESP_OK);
}

// will get the default segment, 0
int ledc_led_speed_get(void) {  
  if (!g_ws2812fx) return(-1);
  ESP_LOGI(TAG,"ledc: get speed %d",g_ws2812fx->getSpeed());
  return(g_ws2812fx->getSpeed() / SPEED_FACTOR);
}


static void blinkWithFx(void *pvParameters) {

  uint16_t mode = FX_MODE_STATIC;

  WS2812FX ws2812fx;
  WS2812FX::Segment *segments = ws2812fx.getSegments();


  ws2812fx.init(NUM_LEDS, leds, false); // type was configured before
  ws2812fx.setBrightness(255);
  ws2812fx.setMode(0 /*segid*/, mode);
  segments[0].colors[0] = 0xff0000;

  g_ws2812fx = &ws2812fx;

  // microseconds
  uint64_t mode_change_time = esp_timer_get_time();

  while (true) {

    if (g_ws2812_mode_random &&
        (mode_change_time + 10000000L) < esp_timer_get_time() ) {
      mode += 1;
      mode %= MODE_COUNT;
      mode_change_time = esp_timer_get_time();
      ws2812fx.setMode(0 /*segid*/, mode);
      printf(" changed mode to %d\n", mode);
    }

    ws2812fx.service();
    vTaskDelay(10 / portTICK_PERIOD_MS); /*10ms*/
  }
};




#if 0
#define N_COLORS 17
CRGB colors[N_COLORS] = { 
  CRGB::AliceBlue,
  CRGB::ForestGreen,
  CRGB::Lavender,
  CRGB::MistyRose,
  CRGB::DarkOrchid,
  CRGB::DarkOrange,
  CRGB::Black,
  CRGB::Red,
  CRGB::Green,
  CRGB::Blue,
  CRGB::White,
  CRGB::Teal,
  CRGB::Violet,
  CRGB::Lime,
  CRGB::Chartreuse,
  CRGB::BlueViolet,
  CRGB::Aqua
};

static void blinkLeds_simple(void *pvParameters){

 	while(1){

		for (int j=0;j<N_COLORS;j++) {

			ESP_LOGI(TAG,"blink leds simple");

			for (int i=0;i<NUM_LEDS;i++) {
			  leds[i] = colors[j];
			}

			FastLED.show();

			vTaskDelay(1000 / portTICK_PERIOD_MS);

		};
	}
};
#endif /* 0 */


// TODO: errors if errors

esp_err_t ledc_init(void) {

  ESP_LOGI(TAG," entering ledc init, call add leds");
  // the WS2811 family uses the RMT driver
  FastLED.addLeds<LED_TYPE, DATA_PIN>(leds, NUM_LEDS);

  // this is a good test because it uses the GPIO ports, these are 4 wire not 3 wire
  //FastLED.addLeds<APA102, 13, 15>(leds, NUM_LEDS);

  ESP_LOGI(TAG," set max power");

  // I have a 2A power supply, although it's 12v
  FastLED.setMaxPowerInVoltsAndMilliamps(12,2000);

  // change the task below to one of the functions above to try different patterns
  ESP_LOGI(TAG,"create task for led action");

  // I think most of the wifi tasks are on 1? or 0? 4000 is enough for tests.... but what about....
  //xTaskCreatePinnedToCore(&ledc_fastfade, "blinkLeds", 6144/*stacksize*/, NULL/*pvparam*/, 10/*pri*/, NULL/*taskhandle*/, 1/*coreid*/);
  xTaskCreatePinnedToCore(&blinkWithFx, "blinkLeds", 1024*8 /*stacksize*/, NULL/*pvparam*/, 5 /*pri*/, NULL/*taskhandle*/, 0/*coreid*/);

  return(ESP_OK);
}
