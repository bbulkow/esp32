/* Blink Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_system.h"
#include "esp_spi_flash.h"

#include "driver/gpio.h"

#include "sdkconfig.h"

/* Can use project configuration menu (idf.py menuconfig) to choose the GPIO to blink,
   or you can edit the following line and set a number here.
   The Menuconfig option is at the top level as a "example configuration".
*/
#define BLINK_GPIO CONFIG_BLINK_GPIO

// this is a variable only because I want to test passing parameters
// to tasks
static int ms_delay = 1500;

void chipinfo() {
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    printf("This is ESP32 chip with %d CPU cores, WiFi%s%s, ",
            chip_info.cores,
            (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
            (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");

    printf("silicon revision %d, ", chip_info.revision);

    printf("%dMB %s flash\n", spi_flash_get_chip_size() / (1024 * 1024),
            (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");
    fflush(stdout);
}

static void blinky_task(void *pvParameters) {
  int ms_to_blink = * (int *) pvParameters;
  ms_to_blink >>= 2;

  printf("Will Blink 30 times then terminate self\n");

  for(int i=0;i<30;i++) {

    /* Blink off (output low) */
    printf("Turning off the LED %d\n",i);
    gpio_set_level(BLINK_GPIO, 0);
    vTaskDelay(ms_to_blink / portTICK_PERIOD_MS);
    /* Blink on (output high) */
    printf("Turning on the LED\n");
    gpio_set_level(BLINK_GPIO, 1);
    vTaskDelay(ms_to_blink / portTICK_PERIOD_MS);

  }

  vTaskDelete(NULL);

}

static void chatty_task(void *pvParameters) {
  int ms_to_chat = * (int *) pvParameters;

  TickType_t now = xTaskGetTickCount();


  printf("Will jaw 20 times then terminate self, and sizeof %d tick %d\n",sizeof(now),(int)now);

  for(int i=0;i<20;i++) {

    /* Blink off (output low) */
    printf("I am a chatty task chatting %d times: %d\n",i,xTaskGetTickCount() );

    vTaskDelay(ms_to_chat / portTICK_PERIOD_MS);

  }

  vTaskDelete(NULL);

}

void app_main()
{


    /* Configure the IOMUX register for pad BLINK_GPIO (some pads are
       muxed to GPIO on reset already, but some default to other
       functions and need to be switched to GPIO. Consult the
       Technical Reference for a list of pads and their default
       functions.)
    */
    gpio_pad_select_gpio(BLINK_GPIO);
    /* Set the GPIO as a push/pull output */
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);

    // I find this pleseant. Maybe we'll find other things
    // that are worth saying at startup.
    chipinfo();

    // Rather sensibly, freeRtos has no concept of auto-termination or
    // auto reboot if you don't have any tasks or exit your app_main.
    // So, let's check these tasks, and if they are both deleted, reboot?
    // Todo: would be good to wire up the watchdog


    // in ESP-IDF is seems there are about 8 tasks
    uint16_t initial_tasks = uxTaskGetNumberOfTasks();

    TaskHandle_t xBlinkyTask;
    TaskHandle_t xChattyTask;

    xTaskCreate(blinky_task, "blink",4096/*stacksizewords*/, 
                (void *) &ms_delay/*param*/, 5 /*pri*/, &xBlinkyTask/*createdtask*/);

    xTaskCreate(chatty_task, "chat",4096/*stacksizewords*/, 
                (void *) &ms_delay/*param*/, 5 /*pri*/, &xChattyTask/*createdtask*/);

    while(1) {

      uint16_t n_tasks = uxTaskGetNumberOfTasks();

      /* let's see how many tasks exist, and terminate when they go away */
      /* future method will be to create a queue and signal back on death */
      // the interesting bit about this talks about the 'idle task' which will
      // reap deleted tasks... it it out there? Did I need to configure
      // something to start it?
      printf("number of tasks remaining: %u waiting for %u\n",n_tasks,initial_tasks);

      if (n_tasks == initial_tasks) {
        printf("my tasks must have died --- let's restart!\n");
        fflush(stdout);
        esp_restart();
      }

      vTaskDelay(500 / portTICK_PERIOD_MS);

    }

  }


