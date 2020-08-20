/* WiFiMulti-idf

** External Interfaces to be called by the application
**
** Copyright Brian Bulkowski, (c) 2020
** brian@bulkowski.org

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"

#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_err.h"

extern "C" {

/*
** NOT EXISTING! Please see the readme and do some coding if you want this supported!
*/
int wifi_multi_ap_remove(const char *ssid);

/*
** Add an AP. If there is no password ( it's open ) you can pass nothing.
** Both values are put into an internal datastructure and are not consumed.
*/

int wifi_multi_ap_add(const char* ssid, const char *password);

/*
** it's very useful to set the log levels programmatically so you can
** see the decisions getting made by the unit and report bugs.
** WARNING - quiet except for disasterous things
** INFO - shows when you attempt to connect, fail to connect, and get IP addresses
** DEBUG - shows info about the choices being made
** VERBOSE shows even more info about the choices being made
*/

void wifi_multi_loglevel_set(esp_log_level_t loglevel);

/*
** call this function BEFORE you add aps to have the background tasks maintain
** the network connection
*/

void wifi_multi_start();

} /* extern C */

