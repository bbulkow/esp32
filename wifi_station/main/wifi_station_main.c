/* WiFi station Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"

#include "esp_system.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"

/* The examples use WiFi configuration that you can set via project configuration menu

   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"
*/
#define EXAMPLE_ESP_WIFI_SSID      CONFIG_ESP_WIFI_SSID
#define EXAMPLE_ESP_WIFI_PASS      CONFIG_ESP_WIFI_PASSWORD
#define EXAMPLE_ESP_MAXIMUM_RETRY  CONFIG_ESP_MAXIMUM_RETRY

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about one event 
 * - are we connected to the AP with an IP? */
const int WIFI_CONNECTED_BIT = BIT0;

static const char *TAG = "wifi station";

static int s_retry_num = 0;

static bool g_is_connected = false;

static void wifi_scan_print()
{
    uint16_t ap_count = 0;
    wifi_ap_record_t *ap_list;
    uint8_t i;
    char *authmode;

    esp_wifi_scan_get_ap_num(&ap_count);    
    printf("--------scan count of AP is %d-------\n", ap_count);
    if (ap_count <= 0)
        return; 

    //malloc every time??? really??? are you just looking for fragmentation trouble?
    ap_list = (wifi_ap_record_t *)malloc(ap_count * sizeof(wifi_ap_record_t));
    if (ap_list == NULL) {
    	printf(" scan: malloc failed, ap_cound %d try later ",(int)ap_count);
    	return;
    }
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_count, ap_list));  

    printf("======================================================================\n");
    printf("             SSID             |    RSSI    |           AUTH           \n");
    printf("======================================================================\n");
    for (i = 0; i < ap_count; i++) 
    {
        switch(ap_list[i].authmode) 
        {
            case WIFI_AUTH_OPEN:
               authmode = "WIFI_AUTH_OPEN";
               break;
            case WIFI_AUTH_WEP:
               authmode = "WIFI_AUTH_WEP";
               break;           
            case WIFI_AUTH_WPA_PSK:
               authmode = "WIFI_AUTH_WPA_PSK";
               break;           
            case WIFI_AUTH_WPA2_PSK:
               authmode = "WIFI_AUTH_WPA2_PSK";
               break;           
            case WIFI_AUTH_WPA_WPA2_PSK:
               authmode = "WIFI_AUTH_WPA_WPA2_PSK";
               break;
            default:
               authmode = "Unknown";
               break;
                }
        printf("%26.26s    |    % 4d    |    %22.22s\n", ap_list[i].ssid, ap_list[i].rssi, authmode);
    }

    free(ap_list);
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base, 
                                int32_t event_id, void* event_data)
{
    if ( event_base == WIFI_EVENT ) {
        switch ( event_id ) {
            case WIFI_EVENT_STA_START:
                ESP_LOGI(TAG, "received STA_START, now connecting");
                esp_wifi_connect();
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
            	g_is_connected = false;
                esp_wifi_connect();
                xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
                s_retry_num++;
                ESP_LOGI(TAG, "retry to connect to the AP");
                break;
            case WIFI_EVENT_SCAN_DONE:
                ESP_LOGI(TAG, "WIFId_EVENT_SCAN_DONE");
                wifi_scan_print();
                break;
            default:
                ESP_LOGI(TAG, "unhandled wifi event %d",event_id);
                break;
        }
    }

    else if (event_base == IP_EVENT) {
    	ip_event_got_ip_t* event;
        switch ( event_id ) {
            case IP_EVENT_STA_GOT_IP:
                event = (ip_event_got_ip_t*) event_data;
                ESP_LOGI(TAG, "got ip:%s",
                         ip4addr_ntoa(&event->ip_info.ip));
                s_retry_num = 0;
                xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
                break;
            default:
                ESP_LOGI(TAG, "unhandled ip event %d",event_id);
                break;
        }
    }
    else {
    	// event base looks like it should be an integer, but it's a structure,
    	// and it's not clear what inside you might print
        ESP_LOGI(TAG, "unknown event base");
    }

}

// Call after esp_wifi_start
static void wifi_scan_task(void *pvParameters) {

    wifi_scan_config_t scan_config = {
            .ssid = 0,
            .bssid = 0,
            .channel = 0,   /* 0--all channel scan */
            .show_hidden = 1,
            .scan_type = WIFI_SCAN_TYPE_ACTIVE,
            .scan_time.active.min = 100,
            .scan_time.active.max = 120,
    };

    while (1)
    {
        ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_config, true));
        vTaskDelay(4000 / portTICK_PERIOD_MS);
    }

}

#define SSID_LEN 32
#define PASSWORD_LEN 64

// this will hold the different APs the client might want to connect to
typedef struct wifi_ap_info_s {
	struct wifi_ap_info_s *next; // single linked list null at end, fairly memory efficient
	char ssid[SSID_LEN]; // chars are more normal than uint8 which esp-idf uses, null terminated
	char password[PASSWORD_LEN]; // again, chars
	wifi_auth_mode_t auth_mode; // should be set by a scan?
		// WIFI_AUTH_OPEN, WIFI_AUTH_WEP, WIFI_AUTH_WPA_PSK, WIFI_AUTH_WAP_WAP@_PSK
		// WIFI_AUTH_WPA2_ENTERPRISE, WIFI_AUTH_WPA3_PSK
	bool tried;
	bool failed;
	wifi_err_reason_t wifi_err; // if failed, last error set here - don't keep trying if wrong password
	int last_rssi;
	// should do last seen time or something, too
} wifi_ap_info_t;

static wifi_ap_info_t *g_wifi_ap_info_head = NULL;

SemaphoreHandle_t g_wifi_ap_info_mutex;

// 0 is success, others are failure
static int wifi_multi_ap_add(const char* ssid, const char *password) {

	if (ssid == NULL)	return(-1);
	if (strlen(ssid) + 1 > SSID_LEN) return(-1);
	// password allowed to be null for open APs
	if (password && ( strlen(password) + 1 > PASSWORD_LEN ) ) return(-1);

	wifi_ap_info_t *ap_info = malloc(sizeof(wifi_ap_info_t));
	if (ap_info == 0) return(-1);

	strcpy(ap_info->ssid, ssid);
	if (password) { // zeroe
		strcpy(ap_info->password, password);
	}
	else {
		ap_info->password[0] = 0;
	}

	ap_info->auth_mode = 0;
	ap_info->tried = false;
	ap_info->failed = false;
	ap_info->wifi_err = 0;
	ap_info->last_rssi = -1000; //??? what is a "invalid" value for rssi? something very negative?

	// need a mutex here -- shouldn't contend much?
	if( pdTRUE == xSemaphoreTake(g_wifi_ap_info_mutex, 1000 / portTICK_PERIOD_MS)) {
		ap_info->next = g_wifi_ap_info_head;
		g_wifi_ap_info_head = ap_info;
		xSemaphoreGive(g_wifi_ap_info_mutex);
	}
	else {
		free(ap_info);
		return(-1);
	}

	return(0);

}

static int wifi_multi_ap_remove(const char *ssid) {

	bool found = false;

	// quickly rip through the datastructure, all in memory access, ok to hold mutex
	if( pdTRUE == xSemaphoreTake(g_wifi_ap_info_mutex, 1000 / portTICK_PERIOD_MS)) {

		wifi_ap_info_t *c = g_wifi_ap_info_head;
		wifi_ap_info_t *prev;

		while (c) {
			if (strcmp( c->ssid, ssid ) == 0 ) {
				if ( c == g_wifi_ap_info_head ) {
					g_wifi_ap_info_head = c->next;
				}
				else {
					c->next = prev;
				}
				free(c);
				found = true;
				break;
			}
			else {
				prev = c;
				c = c->next;
			}
		}

		xSemaphoreGive(g_wifi_ap_info_mutex);
	}
	else {
		ESP_LOGI(TAG, "wifi_multi_ap_remove: could not take mutex");
		return(-1);
	}
	if ( !found ) {
		ESP_LOGI(TAG, "wifi_multi_ap_remove: could not find ssid %s",ssid);
		return(-1);
	}


	return(0);
}


static void wifi_connect_task(void *pvParameters) {

    while(1)
    {
	    // What state am I in? if disconnected, start a connect
	    if (g_is_connected == false) {


		    wifi_config_t wifi_config = {
		        .sta = {
		            .ssid = EXAMPLE_ESP_WIFI_SSID,
		            .password = EXAMPLE_ESP_WIFI_PASS,
		            .scan_method = WIFI_ALL_CHANNEL_SCAN,
		            .sort_method = WIFI_CONNECT_AP_BY_SIGNAL,
		            .threshold.rssi = 127,
		            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
		        },
		    };

		    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );

		    ESP_LOGI(TAG, "connect to ap SSID:%s password:%s",
		             EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);

		}
		vTaskDelay(1000 / portTICK_PERIOD_MS);

    }

}

static TaskHandle_t g_xScanTask;
static TaskHandle_t g_xConnectTask;

// Initis the wifi units, sets up the event loops, and kicks
// off the scanning task and the connecting task

void wifi_multi_start()
{
    s_wifi_event_group = xEventGroupCreate();

    tcpip_adapter_init();

    // need somewhere to statically define mutex. Should be a better way, like how Linux
    // does it, so you don't have to put it in your init code
    // CREATE MUTEX STATIC - static allocations off by default, in the esp32
    // branch. Don't want to make everyone have to turn that on, so going
    // to use dynamic for now.
	g_wifi_ap_info_mutex = xSemaphoreCreateMutex( );

    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t wifi_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_config));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    wifi_country_t wifi_country = {
        .cc = "US",
        .schan = 1,
        .nchan = 11,
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    // this sets a lot of things we may not right at the beginning, like the access point
    // name. Unclear!
    //ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_init_config) );
    ESP_ERROR_CHECK(esp_wifi_set_country(&wifi_country));
    ESP_ERROR_CHECK(esp_wifi_start() );

    // kick off scan and connect tasks
    xTaskCreate(wifi_scan_task, "wifi_scan_task",4096/*stacksizewords*/, 
                (void *) NULL/*param*/, 5 /*pri*/, &g_xScanTask/*createdtask*/);

    xTaskCreate(wifi_connect_task, "connect",4096/*stacksizewords*/, 
                (void *) NULL/*param*/, 5 /*pri*/, &g_xConnectTask/*createdtask*/);

    ESP_LOGI(TAG, "wifi_init_multi finished.");

}



void app_main()
{
    //Initialize NVS --- 
    // NOTE! Does this clear NVS? Or just init access? Looks like gives access because it clears it if there are problems
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    ESP_LOGI(TAG, "ESP_WIFI_MODE_INIT_STA");

    // in ESP-IDF is seems there are about 8 tasks
    uint16_t initial_tasks = uxTaskGetNumberOfTasks();

    wifi_multi_start();
    wifi_multi_ap_add("sisyphus", "!medea4u");
    wifi_multi_ap_add("bb-ap-x", "landshark");
    wifi_multi_ap_add("laertes", "!medea4u");

    // wait till they stop
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
