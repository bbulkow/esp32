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

#include "esp_log.h"
static const char *TAG = "fanc";

#include "fanc.h"

extern "C" {
    void app_main();
}


void app_main(void)
{
    // logging is good
    // Note the following:
    // At compile time: in menuconfig, set the verbosity level using the option CONFIG_LOG_DEFAULT_LEVEL. 
    //    All logging statements for verbosity levels higher than CONFIG_LOG_DEFAULT_LEVEL will be removed by the preprocessor.
    // At runtime: all logs for verbosity levels lower than CONFIG_LOG_DEFAULT_LEVEL are enabled by default. 
    //    The function esp_log_level_set() can be used to set a logging level on a per module basis. 
    //    Modules are identified by their tags, which are human-readable ASCII zero-terminated strings.

    // ESP_LOG_NONE, ESP_LOG_ERROR (error), ESP_LOG_WARN, ESP_LOG_INFO, ESP_LOG_DEBUG, ESP_LOG_VERBOSE
    esp_log_level_set(TAG, ESP_LOG_DEBUG);

    // this seems to help the Wifi unit. Not so sure about that...
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // start the wifi service
    ESP_LOGI(TAG, " starting wifi");
    wifi_multi_start();
    
    wifi_multi_ap_add("sisyphus", "xxx");
    wifi_multi_ap_add("bb-ap-x", "xxx");
    wifi_multi_ap_add("laertes", "xxx");
    ESP_LOGI(TAG, "finished configuring wifi");

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
