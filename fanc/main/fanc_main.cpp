/* LEDC (LED Controller) fade example

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
#include "mdns.h"
#include "esp_sntp.h"

#include "WiFiMulti-idf.h"

#include "esp_err.h"

#include "fanc.h"

extern "C" {
    void app_main();
}


void app_main(void)
{

    // this seems to help the Wifi unit. Not so sure about that...
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);



    // start the wifi service
    printf(" starting wifi\n");
    wifi_multi_start();
    
    wifi_multi_ap_add("sisyphus", "!medea4u");
    wifi_multi_ap_add("bb-ap-x", "landshark");
    wifi_multi_ap_add("laertes", "!medea4u");
    printf("finished configuring wifi\n");

    // why not get network time, if it's out there?
    sntp_set_sync_mode(SNTP_SYNC_MODE_SMOOTH);
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "us.pool.ntp.org");
    sntp_init();

    // start the fanc task
    fanc_init();

    // start the webserver
    webserver_init();

    // start the mdns service, it's helpful
    // going to hard code the name fanc at the moment, seems the project or NVS should have it
    mdns_init();
    mdns_hostname_set("fanc");

    while(1) {


        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
