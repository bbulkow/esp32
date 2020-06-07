/* FANC 

   Fan controller. Uses a very basic web plus rest system to do something basic.

   Will likely be my basic template for doing similar stuff in the future

   This file has the fan control logic. It'll provide some API calls to the main
   parts of the program, and probaby have a task, which either monitors or receives
   messages from other components.

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "nvs_flash.h"

#include "WiFiMulti-idf.h"

#include "esp_err.h"

#include "fanc.h"

//
// The percentage currently set for the configured fan
//

static int g_fanc_percentage = 100; // start at full-on


int fanc_get_percentage(void) {
    return(g_fanc_percentage);
}


esp_err_t fanc_set_percentage(int p) {
    if (p > 100 || p < 0) return(ESP_FAIL);
    g_fanc_percentage = p;
    // update everything
    return(ESP_OK);
}


/*
** use the NVS module to store a value, and get the persistant value on restart
** will simply opulate the global. No point in paying attention to an error
*/

static void fanc_persist_restore(void) {

    nvs_handle_t nvs_h;
    esp_err_t err = nvs_open("fanc", NVS_READONLY, &nvs_h);
    if (err != ESP_OK) {
        printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
        return;
    }

    // Read
    printf("Reading restart counter from NVS ... ");
    int32_t fan_pct = 100; // value will default to 0, if not set yet in NVS
    err = nvs_get_i32(nvs_h, "fan_pct", &fan_pct);
    switch (err) {
        case ESP_OK:
            printf("Retrieved: value is %d\n",fan_pct);
            g_fanc_percentage = fan_pct;
            break;
        case ESP_ERR_NVS_NOT_FOUND:
            printf("The value is not initialized yet!\n");
            break;
        default :
            printf("Error (%s) reading!\n", esp_err_to_name(err));
    }

    // Close
    nvs_close(nvs_h);

    return;
}

/*
** use the NVS module to store a value, and get the persistant value on restart
** will simply opulate the global. No point in paying attention to an error
*/

static void fanc_persist_update(void) {

    nvs_handle_t nvs_h;
    esp_err_t err = nvs_open("fanc", NVS_READWRITE, &nvs_h);
    if (err != ESP_OK) {
        printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
        return;
    }

    // Write
    printf("Writing value to NVS ... ");
    int32_t fan_pct = g_fanc_percentage; // value will default to 0, if not set yet in NVS
    err = nvs_set_i32(nvs_h, "fan_pct", fan_pct);
    switch (err) {
        case ESP_OK:
            printf("set: value is %d\n",fan_pct);
            break;
        case ESP_ERR_NVS_NOT_FOUND:
            printf("The value is not initialized yet!\n");
            break;
        default :
            printf("Error (%s) reading!\n", esp_err_to_name(err));
    }

    printf("Committing updates in NVS ... ");
    err = nvs_commit(nvs_h);
    printf((err != ESP_OK) ? "Failed!\n" : "Done\n");

    // Close
    nvs_close(nvs_h);

    return;
}


/*
 * About this example
 *
 * 1. Start with initializing LEDC module:
 *    a. Set the timer of LEDC first, this determines the frequency
 *       and resolution of PWM.
 *    b. Then set the LEDC channel you want to use,
 *       and bind with one of the timers.
 *
 * 2. You need first to install a default fade function,
 *    then you can use fade APIs.
 *
 * 3. You can also set a target duty directly without fading.
 *
 * 4. This example uses GPIO18/19/4/5 as LEDC output,
 *    and it will change the duty repeatedly.
 *
 * 5. GPIO18/19 are from high speed channel group.
 *    GPIO4/5 are from low speed channel group.
*
*
NOTE:
Here's how duty cycles are specified
Set the LEDC duty, the range of duty setting is [0, (2**duty_resolution)]
Which means:
if your duty resolution is 4 bits, 2 ** 4 is 16, so the range is 0 to 16
If your duty resolution is 12 bits, 2 ** 12 is 4096, so 100 percent is that

Might be nice to have a function that maps percents based on the duty resolution setting

 *
 */

// Only have an esp32, not a S2, so I'm removing the defines for the S2

// pick some values for the example

// I have the suspicion that there is one set of channels for high speed, and one set
// for low speed. 

#define LEDC_TEST_TIMER                LEDC_TIMER_0
//#define LEDC_TEST_SPEED_MODE           LEDC_HIGH_SPEED_MODE
#define LEDC_TEST_SPEED_MODE           LEDC_LOW_SPEED_MODE
#define LEDC_TEST_TIMER_HZ             24000
#define LEDC_TEST_TIMER_RESOLUTION      LEDC_TIMER_10_BIT

#define LEDC_TEST_GPIO        (13)
#define LEDC_TEST_CHANNEL            LEDC_CHANNEL_0


/* this interface works the following way:
** there are several timer channels.
** You'll allocate a timer and configure it for the speed you want ( high, low, hertz etc )
** you'll allocate a channel and assign the channel to a GPIO
** It somewhat seems you might be able to use a single channel to drive multiple GPIO, hard to say
*/

/*
 * Prepare individual configuration
 * for each channel of LED Controller
 * by selecting:
 * - controller's channel number
 * - output duty cycle, set initially to 0
 * - GPIO number where LED is connected to
 * - speed mode, either high or low
 * - timer servicing selected channel
 *   Note: if different channels use one timer,
 *         then frequency and bit_num of these channels
 *         will be the same
 */
ledc_channel_config_t ledc_test_channel = {

        .gpio_num   = LEDC_TEST_GPIO,
        .speed_mode = LEDC_TEST_SPEED_MODE,
        .channel    = LEDC_TEST_CHANNEL,
        .intr_type  = LEDC_INTR_DISABLE, // unclear if this is the fade interrupt or what
        .timer_sel  = LEDC_TIMER_0,      // choose a value you already configured
        .duty       = 0, /* starting duty? */
        .hpoint     = 0, /* no idea what this is for */

};

/* the example had the ordering wrong, but the example was C, which doesn't reorder.
*/

ledc_timer_config_t ledc_test_timer = {
    .speed_mode = LEDC_TEST_SPEED_MODE,          // low speed or high speed
    .duty_resolution = LEDC_TEST_TIMER_RESOLUTION, // resolution of PWM duty
    .timer_num = LEDC_TIMER_0,        // there are 4, let's use channel 0
    .freq_hz = LEDC_TEST_TIMER_HZ,                     // frequency of PWM signal - happen to need 24k for fan control
    .clk_cfg = LEDC_AUTO_CLK            // auto select the source clock
};


static uint32_t duty_cycle_calculate( ledc_timer_bit_t duty_resolution, int percent) {
    if (percent <= 0) return(0); // accuracy
    if (percent >= 100) return (1 << duty_resolution); // might be -1, 
    uint32_t r = 1 << duty_resolution;
    r = (r * percent) / 100; // safe because maximum resolution is 13 bits and we have a lot more
    //printf(" dr %u percent %d is %u\n",duty_resolution,percent,r);
    return (r); 
}

static TaskHandle_t g_fancTask = NULL;
static bool fanc_live = true;

static void fanc_task(void *pvParameters)
{

    esp_err_t err;

    // get prior value from NVS
    fanc_persist_restore();

    // set up timer0
    err = ledc_timer_config(&ledc_test_timer);
    if (err == ESP_OK) printf(" succeeded configing timer\n");
    else printf(" could not configure timer: error %d\n",err);

    // Set up controller 0, which will start the pin outputting to level 0-
    err = ledc_channel_config(&ledc_test_channel);
    if (err == ESP_OK) printf(" succeeded configing channel\n");
    else printf(" could not configure channel: error %d\n",err);

    // don't want the ledc service yet
    // Initialize fade service.--- let's not get too fancy yet
    //ledc_fade_func_install(0);

    int last_value = -1;

    while (fanc_live) {


        if (last_value != g_fanc_percentage) {

            printf(" changing fan duty cycle: old value was %d new will be %d\n",last_value,g_fanc_percentage);

            // use the most recent value
            err = ledc_set_duty(LEDC_TEST_SPEED_MODE, LEDC_TEST_CHANNEL, 
                duty_cycle_calculate(LEDC_TEST_TIMER_RESOLUTION, g_fanc_percentage ));
            //if (err == ESP_OK) printf(" succeeded setting duty\n");
            //else printf(" failed setting duty: error %d\n",err);

            err = ledc_update_duty(LEDC_TEST_SPEED_MODE, LEDC_TEST_CHANNEL);
            //if (err == ESP_OK) printf(" succeeded updating duty\n");
            //else printf(" failed updating duty: error %d\n",err);

            // write new value
            fanc_persist_update();

            last_value = g_fanc_percentage;

        }

        // secs between so you can tell what's happening
        vTaskDelay(1000 / portTICK_PERIOD_MS);


#if 0
        printf("LEDC increase duty without fade\n");
        for (int i = 0; i <= 100; i+=3) {

            printf("LEDC increase duty = percent %d without fade\n", i);

            err = ledc_set_duty(LEDC_TEST_SPEED_MODE, LEDC_TEST_CHANNEL, duty_cycle_calculate(LEDC_TEST_TIMER_RESOLUTION, i ));
            //if (err == ESP_OK) printf(" succeeded setting duty\n");
            //else printf(" failed setting duty: error %d\n",err);

            err = ledc_update_duty(LEDC_TEST_SPEED_MODE, LEDC_TEST_CHANNEL);
            //if (err == ESP_OK) printf(" succeeded updating duty\n");
            //else printf(" failed updating duty: error %d\n",err);

            // secs between so you can tell what's happening

            vTaskDelay(400 / portTICK_PERIOD_MS);
        }
#endif

    }
}

esp_err_t fanc_init(void) {

    // kick off scan and connect tasks
    xTaskCreate(fanc_task, "fanc_task",4096/*stacksizewords*/, 
                (void *) NULL/*param*/, 5 /*pri*/, &g_fancTask/*createdtask*/);

    return(ESP_OK);
}

void fanc_destroy(void) {
    fanc_live = false;
}

