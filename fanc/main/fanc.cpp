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
#include "driver/gpio.h"
#include "nvs_flash.h"

#include "WiFiMulti-idf.h"

#include "esp_err.h"

#include "fanc.h"

//
// These pins and values are for generating a PWM code 
// for driving a "intel format" PWM fan.
//

#define LEDC_FANC_TIMER                LEDC_TIMER_0
#define LEDC_FANC_SPEED_MODE           LEDC_LOW_SPEED_MODE
#define LEDC_FANC_TIMER_HZ             24000
#define LEDC_FANC_TIMER_RESOLUTION      LEDC_TIMER_8_BIT


#define LEDC_FANC_GPIO              (18)
#define LEDC_FANC_CHANNEL            LEDC_CHANNEL_0

//
// These values are for measuring the speed of the fan
//
#define FANC_PULSE_COUNT_GPIO       (GPIO_NUM_19)
#define FANC_PULSE_COUNT_PIN_SEL  (1ULL << FANC_PULSE_COUNT_GPIO)



//
// The percentage currently set for the configured fan
//

static int g_fanc_percentage = 100; // start at full-on


int fanc_percentage_get(void) {
    return(g_fanc_percentage);
}


esp_err_t fanc_percentage_set(int p) {
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
** reading the speed of a fan
**
** esp_err_t gpio_intr_enable(gpio_num_t gpio_num) --- enables interrupts?
**    get the current value
** int gpio_get_level(gpio_num_t gpio_num)
**    set direction to input
** esp_err_t gpio_set_direction(gpio_num_t gpio_num, gpio_mode_t mode)
**    allocate an interrupt handler for the GPIO in question?

** esp_err_t gpio_isr_handler_add(gpio_num_t gpio_num, gpio_isr_t isr_handler, void *args)
**     remove the handler?
** esp_err_t gpio_isr_handler_remove(gpio_num_t gpio_num)

**    set the interrupt type, that's how you get notified on rise, fall, etc
**    GPIO_INTR_POSEDGE , ANYEDGE, etc
** esp_err_t gpio_set_intr_type(gpio_num_t gpio_num, gpio_int_type_t intr_type);
**
** thus - pick a pin. Register a handler. Set the interrupt type.
**   increment a global when called and returned, that simple, now you have a counter
**   every time a rising edge happens

 https://www.lucadentella.it/en/2017/02/25/esp32-12-io-e-interrupts/
 */

// This global will be incremented when the interrupt fires
static uint32_t g_fanc_pulse_count = 0;
static uint32_t g_fanc_previous_pulse_count = 0;
static int64_t  g_fanc_previous_time = 0;
static float g_fanc_rps = 0.0;


static void IRAM_ATTR fanc_pulse_count_isr(void *p) {
    g_fanc_pulse_count++;
}

// somewhat annoyingly, this will measure the 

float fanc_speed_get(void) {

	if (g_fanc_previous_time == 0) return(0.0);

	return (g_fanc_rps);
}

// so annoying these must be the same order as the struct definition

static gpio_config_t pulse_count_gpio = {
    .pin_bit_mask = FANC_PULSE_COUNT_PIN_SEL,
    .mode = GPIO_MODE_INPUT,
    .pull_up_en = GPIO_PULLUP_DISABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type = GPIO_INTR_POSEDGE,
};

static esp_err_t fanc_pulse_count_init(void) {

    esp_err_t   err;

    err = gpio_config(&pulse_count_gpio);
    if (err != ESP_OK) {
        printf(" fanc_plus_count: could not configure gpio config\n");
        return(err);
    }

    //gpio_set_direction(FANC_PULSE_COUNT_GPIO, GPIO_MODE_INPUT);
    //gpio_set_intr_type(FANC_PULSE_COUNT_GPIO, GPIO_INTR_POSEDGE);

    // example does not, a bit odd?
    err = gpio_install_isr_service(0 /* default */);
    if (err != ESP_OK) {
        printf(" fanc_plus_count: could not add isr handler\n");
        return(err);
    }

    err = gpio_isr_handler_add(FANC_PULSE_COUNT_GPIO, fanc_pulse_count_isr, 0 /* param for isr */);
    if (err != ESP_OK) {
        printf(" fanc_plus_count: could not add isr handler\n");
        return(err);
    }

    err = gpio_intr_enable(FANC_PULSE_COUNT_GPIO);
    if (err != ESP_OK) {
        printf(" fanc_plus_count: could enable isr\n");
        return(err);
    }

    printf(" fanc_pulse: successful init\n");
    return(ESP_OK);
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
static ledc_channel_config_t ledc_fanc_channel = {
        .gpio_num   = LEDC_FANC_GPIO,
        .speed_mode = LEDC_FANC_SPEED_MODE,
        .channel    = LEDC_FANC_CHANNEL,
        .intr_type  = LEDC_INTR_DISABLE, // unclear if this is the fade interrupt or what
        .timer_sel  = LEDC_FANC_TIMER,      // choose a value you already configured
        .duty       = 0, /* starting duty? */
        .hpoint     = 0, /* no idea what this is for */
};

/* the example had the ordering wrong, but the example was C, which doesn't reorder.
*/

static ledc_timer_config_t ledc_fanc_timer = {
    .speed_mode = LEDC_FANC_SPEED_MODE,          // low speed or high speed
    .duty_resolution = LEDC_FANC_TIMER_RESOLUTION, // resolution of PWM duty
    .timer_num = LEDC_FANC_TIMER,        // there are 4, let's use channel 0
    .freq_hz = LEDC_FANC_TIMER_HZ,                     // frequency of PWM signal - happen to need 24k for fan control
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
    err = ledc_timer_config(&ledc_fanc_timer);
    if (err == ESP_OK) printf(" succeeded configing timer\n");
    else printf(" could not configure timer: error %d\n",err);

    // Set up controller 0, which will start the pin outputting to level 0-
    err = ledc_channel_config(&ledc_fanc_channel);
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
            err = ledc_set_duty(LEDC_FANC_SPEED_MODE, LEDC_FANC_CHANNEL, 
                duty_cycle_calculate(LEDC_FANC_TIMER_RESOLUTION, g_fanc_percentage ));
            //if (err == ESP_OK) printf(" succeeded setting duty\n");
            //else printf(" failed setting duty: error %d\n",err);

            err = ledc_update_duty(LEDC_FANC_SPEED_MODE, LEDC_FANC_CHANNEL);
            //if (err == ESP_OK) printf(" succeeded updating duty\n");
            //else printf(" failed updating duty: error %d\n",err);

            // write new value
            fanc_persist_update();
            last_value = g_fanc_percentage;

        }

        // interesting thing here is the number of pulses seems wrong by almost
        // every measure... is 37 revs per second a reasonable number? 

        // how fast am I spinning?
        uint32_t pulse_count = g_fanc_pulse_count;
        int64_t  now = esp_timer_get_time(); // microseconds?


		//printf("pulse counter change: %u was %u now %u\n", pulse_count - g_fanc_previous_pulse_count, g_fanc_previous_pulse_count, pulse_count);
		g_fanc_rps = ((float)(pulse_count - g_fanc_previous_pulse_count)) / ((now - g_fanc_previous_time) / 1000000.0);
		//printf(" revs per second: %f\n",g_fanc_rps);
        g_fanc_previous_pulse_count = pulse_count;
        g_fanc_previous_time = now;

        // If you go too fast, the measurement will be poor, and we'll have to skip. Similarly,
        // updating the fan speed too often's probably bad. I like a third of a second.
        vTaskDelay(480 / portTICK_PERIOD_MS);


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

    // input to grab sense pulses so we know how fast it's going
    fanc_pulse_count_init();

    // kick off scan and connect tasks
    xTaskCreate(fanc_task, "fanc_task",4096/*stacksizewords*/, 
                (void *) NULL/*param*/, 5 /*pri*/, &g_fancTask/*createdtask*/);



    return(ESP_OK);
}

void fanc_destroy(void) {
    fanc_live = false;
}

