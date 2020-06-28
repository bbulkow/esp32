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


void ChangePalettePeriodically(){

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

void blinkLeds_interesting(void *pvParameters){
  while(1){
  	 ESP_LOGI(TAG,"blink leds");
    ChangePalettePeriodically();
    
    static uint8_t startIndex = 0;
    startIndex = startIndex + 1; /* motion speed */
    
    for( int i = 0; i < NUM_LEDS; i++) {
        leds[i] = ColorFromPalette( currentPalette, startIndex, 64, currentBlending);
        startIndex += 3;
    }
    ESP_LOGI(TAG,"show leds\n");
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

void blinkLeds_simple(void *pvParameters){

 	while(1){

		for (int j=0;j<N_COLORS;j++) {

			ESP_LOGI(TAG,"blink leds");

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

void blinkLeds_chase(void *pvParameters) {
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
  ESP_LOGI(TAG,"create task for led blinking");
  xTaskCreatePinnedToCore(&blinkLeds_simple, "blinkLeds", 6000/*stacksize*/, NULL/*pvparam*/, 5/*pri*/, NULL/*taskhandle*/, tskNO_AFFINITY/*coreid*/);

  return(ESP_OK);
}
