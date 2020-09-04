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

typedef struct {
  int sweep;
  CRGB color;
} ledc_fastfade2_t;

static void _ledc_fastfade2_cb(void *param){

  ledc_fastfade2_t *ff = (ledc_fastfade2_t *)param;
  int sweep = ff->sweep % 3;


  switch (sweep) {
    // red
    case 0:
      ff->color.red+=2;
      if (ff->color.red == 255) {
        ff->sweep++;
        ff->color.red = 0;
      }
      break;
    // greem
    case 1:
      ff->color.green+=2;
      if (ff->color.green == 255) {
        ff->sweep++;
        ff->color.green = 0;
      }
      break;
    //blue
    case 2:
      ff->color.blue+=2;
      if (ff->color.blue == 255) {
        ff->sweep++;
        ff->color.blue = 0;
      }
      break;
  }

  //ESP_LOGI(TAG,"fast rgb fade r: %d g: %d b: %d",ff->color.red,ff->color.green, ff->color.blue);

  fill_solid(leds,NUM_LEDS,ff->color);

  FastLED.show();

};

static void ledc_fastfade(void *pvParameters){

  ledc_fastfade1_t ff1_t = {
    .color = CHSV(0/*hue*/,255/*sat*/,255/*value*/)
  };

  ledc_fastfade2_t ff2_t = {
    .sweep = 0,
    .color = CRGB(0/*red*/,0/*green*/,0/*blue*/)
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


static void ChangePalettePeriodically(){

  int64_t now = esp_timer_get_time();
  uint8_t secondHand = (now / 1000000L ) % 60;
  static uint8_t lastSecond = 99;

  if( lastSecond != secondHand) {
    lastSecond = secondHand;
    if( secondHand ==  0)  { currentPalette = RainbowColors_p;         currentBlending = LINEARBLEND; }
    if( secondHand == 10)  { currentPalette = RainbowStripeColors_p;   currentBlending = NOBLEND;  }
    if( secondHand == 15)  { currentPalette = RainbowStripeColors_p;   currentBlending = LINEARBLEND; }
    if( secondHand == 20)  { SetupPurpleAndGreenPalette();             currentBlending = LINEARBLEND; }
    if( secondHand == 25)  { SetupTotallyRandomPalette();              currentBlending = LINEARBLEND; }
    if( secondHand == 30)  { SetupBlackAndWhiteStripedPalette();       currentBlending = NOBLEND; }
    if( secondHand == 35)  { SetupBlackAndWhiteStripedPalette();       currentBlending = LINEARBLEND; }
    if( secondHand == 40)  { currentPalette = CloudColors_p;           currentBlending = LINEARBLEND; }
    if( secondHand == 45)  { currentPalette = PartyColors_p;           currentBlending = LINEARBLEND; }
    if( secondHand == 50)  { currentPalette = myRedWhiteBluePalette_p; currentBlending = NOBLEND;  }
    if( secondHand == 55)  { currentPalette = myRedWhiteBluePalette_p; currentBlending = LINEARBLEND; }
  }

}

static void blinkLeds_interesting(void *pvParameters){
  while(1){
  	 ESP_LOGI(TAG,"blink leds interesting");
    ChangePalettePeriodically();
    
    static uint8_t startIndex = 0;
    startIndex = startIndex + 1; /* motion speed */
    
    for( int i = 0; i < NUM_LEDS; i++) {
        leds[i] = ColorFromPalette( currentPalette, startIndex, 64, currentBlending);
        startIndex += 3;
    }
    ESP_LOGI(TAG,"show leds");
    FastLED.show();

    vTaskDelay(400 / portTICK_PERIOD_MS);
  };

};

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

#define N_COLORS_CHASE 7
CRGB colors_chase[N_COLORS_CHASE] = { 
  CRGB::AliceBlue,
  CRGB::Lavender,
  CRGB::DarkOrange,
  CRGB::Red,
  CRGB::Green,
  CRGB::Blue,
  CRGB::White,
};

static void blinkLeds_chase(void *pvParameters) {
  int pos = 0;
  int led_color = 0;
  while(1){

  	ESP_LOGI(TAG,"chase leds");

  		// do it the dumb way - blank the leds
	    for (int i=0;i<NUM_LEDS;i++) {
	      leds[i] =   CRGB::Black;
	    }

	    // set the one LED to the right color
	    leds[pos] = colors_chase[led_color];
	    pos = (pos + 1) % NUM_LEDS;

	    // use a new color
	    if (pos == 0) {
	    	led_color = (led_color + 1) % N_COLORS_CHASE ;
	    }

	    uint64_t start = esp_timer_get_time();
	    FastLED.show();
	    uint64_t end = esp_timer_get_time();

	    ESP_LOGI(TAG,"Show Time: %" PRIu64 ,end-start);

      vTaskDelay(200 / portTICK_PERIOD_MS);
	};

}

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

//  xTaskCreatePinnedToCore(&blinkLeds_simple, "blinkLeds", 4000/*stacksize*/, NULL/*pvparam*/, 5/*pri*/, NULL/*taskhandle*/, tskNO_AFFINITY/*coreid*/);

  //xTaskCreatePinnedToCore(&ledc_fastfade, "blinkLeds", 4000/*stacksize*/, NULL/*pvparam*/, 5/*pri*/, NULL/*taskhandle*/, tskNO_AFFINITY/*coreid*/);
  xTaskCreatePinnedToCore(&ledc_fastfade, "blinkLeds", 6144/*stacksize*/, NULL/*pvparam*/, 10/*pri*/, NULL/*taskhandle*/, 1/*coreid*/);

  return(ESP_OK);
}
