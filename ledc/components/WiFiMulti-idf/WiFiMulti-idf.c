/* WiFi station example

** but with continual scanning of multiple APs
** so you can easily connect to different ones
** attempting to support ESP-IDF 4.0 and upward
**
** Portions copyright Brian Bulkowski, (c) 2020
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
#include "esp_event.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_idf_version.h"  // several bits of code depend on version :-(

#include "lwip/err.h"
#include "lwip/sys.h"

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(4,1,0)
    This file uses the new ESP-IDF networking system, so only supports ESP-IDF 4.1 and later.
#endif

/*
** a quick note about scanning and memory use.
** the esp_idf scan interface is poorly defined. You execute a scan, you get notified that it's
** complete, then you must call esp_wifi_scan_get_ap_records(), because that copies the details
** into your own buffer, and also frees the dynamic memory associated with the scan.
** There is no description of what happens if you call the scan and not the 'get_ap_records'.
** There is also a potential race because you have to get the number of records then get the
** records itself, so you better not be doing a background scan at the same time.
**
** While this interface is fragile, it works, just if you are doing async programming, you need to make sure
** you don't start a new scan until you pull the old scan data out, or something similar.
**
** Note to espressif. You should define in your documentation what happens if you don't call 'get_ap_records'
** after a scan, repeatedly. I have raised an issue in the github repo but whatever.
**
** I think the safest thing is a mutex to make sure there isn't concurrent access. I think trying to make
** certain there is a 1-for-1 access ot that function is too hard.
**
** Interesting! It's a really bad idea to do a scan and be connected, because it seems there
** is generally only one radio. With only one radio, a scan, even a background scan, that
** seems benign, will cause large packet delays. Unclear _exactly_ why that is, and it's
** kinda cool that it just doesn't fail, but the real action here should be to scan as fast as
** possible when not connected, and every time a connection fails, scan again.
**
** the "find best" logic is still good, and maybe even the idea that you should keep an old scan
** if it's only a few days old and try and immediate reconnect, but you can't background scan.
*/

/*
** NOTE TO SELF! ESP_ERROR_CHECK() calls ASSERT if there is an error, which
** almost certainly results in a reboot. If an error is recoverable, 
** ESP_OK is generally the OK state for all the esp_ calls, and see this page
** about it all.... 
** https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/error-handling.html
*/

/* The examples use WiFi configuration that you can set via project configuration menu

   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"
*/
#define EXAMPLE_ESP_WIFI_SSID      CONFIG_ESP_WIFI_SSID
#define EXAMPLE_ESP_WIFI_PASS      CONFIG_ESP_WIFI_PASSWORD
#define EXAMPLE_ESP_MAXIMUM_RETRY  CONFIG_ESP_MAXIMUM_RETRY

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* there seem to be a new thing, the esp_netif_t, which you need to attach to the wifi system.
** I don't think it happens by default anymore, which is why I am not getting an IP address
*/
static esp_netif_t *g_wifi_netif = 0;

/* The event group allows multiple bits for each event, but we only care about one event 
 * - are we connected to the AP with an IP? */
const int WIFI_CONNECTED_BIT = BIT0;

static const char *TAG = "wifi station";

static int s_retry_num = 0;

static bool g_is_connected = false;
static bool g_is_connecting = false;
static bool g_is_scanning = false;

// using a microsecond timer, make it ten seconds?
#define WIFI_STALE_TIME (20L * 1000L * 1000L)


///
/// local structures
//

#define SSID_LEN 32
#define PASSWORD_LEN 64

// this will hold the different APs the client might want to connect to
typedef struct wifi_ap_info_s {
    struct wifi_ap_info_s *next; // single linked list null at end, fairly memory efficient
    uint8_t ssid[SSID_LEN]; // chars are more normal than uint8 which esp-idf uses, null terminated
    uint8_t password[PASSWORD_LEN]; // again, chars
    wifi_auth_mode_t authmode; // should be set by a scan?
        // WIFI_AUTH_OPEN, WIFI_AUTH_WEP, WIFI_AUTH_WPA_PSK, WIFI_AUTH_WAP_WAP@_PSK
        // WIFI_AUTH_WPA2_ENTERPRISE, WIFI_AUTH_WPA3_PSK
    int   successes;
    int   fails;
    wifi_err_reason_t wifi_err; // if failed, last error set here - don't keep trying if wrong password
                                // it turns out that password is "4th handshake" usually.
    int             last_rssi;
    int64_t         last_seen; // microsec ticks since last seen
} wifi_ap_info_t;

static wifi_ap_info_t *g_wifi_ap_info_head = NULL;

SemaphoreHandle_t g_wifi_ap_info_mutex;

// the ESP interface is unclear about whether these 
// functions are thread safe against each other, and rather 
// implies they are not. Create a mutex.
SemaphoreHandle_t g_wifi_scan_mutex;

//
/// forward references
//

static wifi_ap_info_t *wifi_multi_find(const uint8_t *ssid);


static const char *get_authmode_str(int authmode_id) {
    switch(authmode_id) {
        case WIFI_AUTH_OPEN:
           return( "WIFI_AUTH_OPEN" );
        case WIFI_AUTH_WEP:
           return( "WIFI_AUTH_WEP" );
        case WIFI_AUTH_WPA_PSK:
           return( "WIFI_AUTH_WPA_PSK" );
        case WIFI_AUTH_WPA2_PSK:
           return( "WIFI_AUTH_WPA2_PSK" );
        case WIFI_AUTH_WPA_WPA2_PSK:
           return( "WIFI_AUTH_WPA_WPA2_PSK");
    }
    return("Unknown");
}

//
// Call this after a successful scan to update internal data structures
// so we can record what we're within range of when we try to connect
//
// If you also want information printed, set print to true
//

static void wifi_scan_update(bool doprint)
{
    uint16_t ap_count = 0;
    wifi_ap_record_t *ap_list;
    uint8_t i;

    int64_t now = esp_timer_get_time(); // time in microseconds since boot

    // we want to make sure a scan isn't working at the same time as this.
    // it's unclear what might happen in the backgroun.
    // at least, lets not execute a scan at the same time.
    if( pdFALSE == xSemaphoreTake(g_wifi_scan_mutex, 1000 / portTICK_PERIOD_MS)) {
        ESP_LOGW(TAG," could not update AP list, could not take scan mutex");
        ESP_LOGW(TAG," could result in a memory leak, because not calling scan_get_ap_records?");
        return;
    }

    esp_wifi_scan_get_ap_num(&ap_count);    
    if (doprint) { printf("--------scan count of AP is %d-------\n", ap_count); }
    if (ap_count <= 0) {
        xSemaphoreGive(g_wifi_scan_mutex);
        return; 
    }

    //malloc every time??? really??? are you just looking for fragmentation trouble?
    ap_list = (wifi_ap_record_t *)malloc(ap_count * sizeof(wifi_ap_record_t));
    if (ap_list == NULL) {
    	ESP_LOGW(TAG," scan: malloc failed, ap_count %d try later ",(int)ap_count);
        xSemaphoreGive(g_wifi_scan_mutex);
    	return;
    }
    // this might not be the best idea, aren't there
    // cases you'd just like to continue? Throws if error.
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_count, ap_list));  

    // don't need to hold the mutex while copying
    xSemaphoreGive(g_wifi_scan_mutex);

    // regarding the best one, this can get tricky, because there might be
    // multiple APs with the same name. To handle this, if the time is the same,
    // only make the RSSI better

    if (doprint) {
        printf("======================================================================\n");
        printf("             SSID             |    RSSI    |           AUTH           \n");
        printf("======================================================================\n");
    }

    for (i = 0; i < ap_count; i++) 
    {
        if (doprint) {
            printf("%26.26s    |    % 4d    |    %22.22s\n", ap_list[i].ssid, ap_list[i].rssi, get_authmode_str(ap_list[i].authmode) );
        } /* doprint */

        // update the stats we have about this ap
        ESP_LOGV(TAG," scan: updating stats for ssid %s",ap_list[i].ssid);
        wifi_ap_info_t *ap_info = wifi_multi_find(ap_list[i].ssid);
        if (ap_info) {
            ap_info->authmode = ap_list[i].authmode;
            if (ap_info->last_seen < now) {
                ap_info->last_rssi = ap_list[i].rssi;
                ap_info->last_seen = now;
                ESP_LOGV(TAG," scan: updated stats for ssid %s seen %lld rssi %d",
                    ap_list[i].ssid,ap_info->last_seen,ap_info->last_rssi);
            }
            // multiple APs with same name, take best RSSI
            else {
                ESP_LOGV(TAG," scan: last seen is now? only update RSSI: ssid %s",ap_list[i].ssid);
                if (ap_info->last_rssi < ap_list[i].rssi) {
                    ap_info->last_rssi = ap_list[i].rssi;
                }
            }
        }
        else {
            ESP_LOGV(TAG," scan: can't update ssid %s not found",(const char *)ap_list[i].ssid);
        }
    }

    free(ap_list);
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base, 
                                int32_t event_id, void* event_data)
{
    if ( event_base == WIFI_EVENT ) {
        switch ( event_id ) {
            case WIFI_EVENT_STA_START:
                ESP_LOGD(TAG, "received STA_START, now connecting");
                g_is_connected = false;
                g_is_connecting = false;
                break;
            case WIFI_EVENT_STA_CONNECTED:
            {
                wifi_event_sta_connected_t *ev_conn = (wifi_event_sta_connected_t *)event_data;
                ESP_LOGD(TAG, "received STA_CONNECTED channel %d",ev_conn->channel);

                // Look up ssid, mark as success
                wifi_ap_info_t *ap = wifi_multi_find( ev_conn->ssid );
                if (ap) {
                    ap->successes++;
                }

                g_is_connected = true;
                g_is_connecting = false;
                break;
            }
            case WIFI_EVENT_STA_DISCONNECTED:
            {
                wifi_event_sta_disconnected_t *ev_dis = (wifi_event_sta_disconnected_t *)event_data;

                ESP_LOGI(TAG, "EVENT_STA_DISCONNECTED: ssid %s reason %d: retry to connect to the AP?",ev_dis->ssid, ev_dis->reason);
                if (ev_dis->reason == WIFI_REASON_AUTH_FAIL) {
                    ESP_LOGI(TAG, "EVENT_STA_DISCONNECTED: reason %d: auth fail, not likely!",ev_dis->reason);
                }
                // Look up ssid, mark as failed
                wifi_ap_info_t *ap = wifi_multi_find( ev_dis->ssid );
                if (ap) {
                    ap->fails++;
                }

                // TODO: signal for a new scan
                g_is_connected = false;
                g_is_connecting = false;

                break;
            }
            case WIFI_EVENT_SCAN_DONE:
            {
                g_is_scanning = false;
                wifi_event_sta_scan_done_t *ev_sc = (wifi_event_sta_scan_done_t *)event_data;
                ESP_LOGD(TAG, "WIFId_EVENT_SCAN_DONE status %d numaps %d",ev_sc->status,ev_sc->number);
                if (ev_sc->status == 0) { // 0 is success
                    // should do something fancier --- look at the log level???
                    wifi_scan_update(false/*print*/);
                }
                break;
            }
            default:
                ESP_LOGI(TAG, "unhandled wifi event %d",event_id);
                break;
        }
    }

// NB: below is the ESP function 'IP2STR'. This composes the IP address into 4 ints and thus must be used
// with a format string of %d.%d.%d.%d, which is what IPSTR is. This is a supported ESP function despite the poor prefix
// the alternate is 'esp_ip4addr_ntoa', which creates a string, and ints are better than strings, admittedly.
// the underlying function for parsing out the bytes is: esp_ip4_addr1_16(ipaddr) (1,2,3,4)

    else if (event_base == IP_EVENT) {
    	ip_event_got_ip_t* event;
        switch ( event_id ) {
            case IP_EVENT_STA_GOT_IP:
                event = (ip_event_got_ip_t*) event_data;
                ESP_LOGI(TAG, "got ip:" IPSTR,
                         IP2STR(&event->ip_info.ip));
                s_retry_num = 0;
                xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
                break;
            case IP_EVENT_STA_LOST_IP:
                // informative really
                ESP_LOGI(TAG, "lost ip:" );
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
            .scan_type = WIFI_SCAN_TYPE_PASSIVE,   // active scans beacon out and cause network traffic.
                                                    // in most environments, you probably want a passive scan
            .scan_time.active.min = 80,
            .scan_time.active.max = 120,
    };

    while (1)
    {

        // I am worried this is not thread safe, so protect
        if( pdTRUE == xSemaphoreTake(g_wifi_scan_mutex, 1000 / portTICK_PERIOD_MS)) {
            // I read you should not scan while connecting
            if ((g_is_connected) == false && (g_is_connecting == false)) {
                ESP_LOGD(TAG, "scan started: ");
                esp_err_t err = esp_wifi_scan_start(&scan_config, true);
                if (err == ESP_OK) {
                    g_is_scanning = true;
                }
            }
            else {
                ESP_LOGV(TAG, "skipping scan while connecting or connected ");
            }
            xSemaphoreGive(g_wifi_scan_mutex);
        }
        // test to see if things change
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }

}

// functions for SSID, which are defined as unsigned chars not signed chars
// returns size copied
static inline int u8cmp( const uint8_t *s1, const uint8_t *s2) {
    return( strcmp( (const char *)s1, (const char *) s2));
}

// returns pointer to destination
static inline char *u8cpy( uint8_t *dst, const uint8_t *src) {
    return( strcpy( (char *)dst, (const char *) src));
}

// Finds an AP in the list
// not yet thread safe and we're using threads! Assumes all are registered
// forever --- fixme, make that structure reference counted
//
// INTERNAL
static wifi_ap_info_t *
wifi_multi_find(const uint8_t *ssid)
{
    wifi_ap_info_t *r = 0;

    // quickly rip through the datastructure, all in memory access, ok to hold mutex
    if( pdTRUE == xSemaphoreTake(g_wifi_ap_info_mutex, 1000 / portTICK_PERIOD_MS)) {

        wifi_ap_info_t *c = g_wifi_ap_info_head;

        while (c) {
            if (u8cmp( c->ssid, ssid ) == 0 ) {
                r = c;
                break;
            }
            else {
                c = c->next;
            }
        }

        xSemaphoreGive(g_wifi_ap_info_mutex);
    }
    else {
        ESP_LOGD(TAG, "could not take wifi semapore for a find, failing");
        return( NULL );
    }

    return(r);

}


// 0 is success, others are failure
esp_err_t wifi_multi_ap_add(const char* ssid, const char *password) {

	if (ssid == NULL)	return(-1);
	if (strlen(ssid) + 1 > SSID_LEN) return(ESP_ERR_INVALID_ARG);
	// password allowed to be null for open APs
	if (password && ( strlen(password) + 1 > PASSWORD_LEN ) ) return(ESP_ERR_INVALID_ARG);

	wifi_ap_info_t *ap_info = malloc(sizeof(wifi_ap_info_t));
	if (ap_info == 0) return(ESP_ERR_NO_MEM);

	u8cpy(ap_info->ssid, (const uint8_t *) ssid);

	if (password)      u8cpy(ap_info->password, (const uint8_t *) password);
	else               ap_info->password[0] = 0;

	ap_info->authmode = 0;
	ap_info->successes = 0;
	ap_info->fails = 0;
	ap_info->wifi_err = 0;
	ap_info->last_rssi = -1000; //??? what is a "invalid" value for rssi? something very negative?
    ap_info->last_seen = 0;

	// need a mutex here -- shouldn't contend much?
	if( pdFALSE == xSemaphoreTake(g_wifi_ap_info_mutex, 1000 / portTICK_PERIOD_MS)) {
        free(ap_info);
        return(ESP_FAIL); // no obvious case here, should almost throw error
    }

	ap_info->next = g_wifi_ap_info_head;
	g_wifi_ap_info_head = ap_info;
	xSemaphoreGive(g_wifi_ap_info_mutex);

	return(0);

}

esp_err_t wifi_multi_ap_remove(const char *ssid) {
    ESP_LOGW(TAG, "wifi_multi_ap_remove not supported, leaking memory");
    return(ESP_OK);
}

// right now, I don't properly protect this data structure.
// I hand around the allocated SSID structures without a care in the world
// There fore I can't really remove one .... can only add this
// if the code gets a little more complex
#if 0
int wifi_multi_ap_remove(const char *ssid) {

	bool found = false;

	// quickly rip through the datastructure, all in memory access, ok to hold mutex
	if( pdTRUE == xSemaphoreTake(g_wifi_ap_info_mutex, 1000 / portTICK_PERIOD_MS)) {

		wifi_ap_info_t *c = g_wifi_ap_info_head;
		wifi_ap_info_t *prev;

		while (c) {
			if (u8cmp( c->ssid, (const uint8_t *) ssid ) == 0 ) {
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
#endif // ap_remove

//
// This has the magic because it'll look through and find the best combination of 
// signal strength and recent. Eventually also avoid ones that you've failed with.

static wifi_ap_info_t *
wifi_multi_find_best()
{
    wifi_ap_info_t *r = 0;
    int64_t now = esp_timer_get_time();

    if( pdFALSE == xSemaphoreTake(g_wifi_ap_info_mutex, 1000 / portTICK_PERIOD_MS)) {
        ESP_LOGW(TAG,"Could not get ap info mutex, timedout");
        return(0);
    }

    // quickly rip through the datastructure, all in memory access, ok to hold mutex
    wifi_ap_info_t *c = g_wifi_ap_info_head;

    while (c) {
        // make sure I've seen it at least once before
        if (c->last_seen == 0) {
            ESP_LOGV(TAG," ssid %s: skipping not seen yet",(const char *)c->ssid);
            goto NEXT;
        }
        // make sure if its encrypted I have the username and password
        if ( (c->password[0] == 0) &&
             (c->authmode != WIFI_AUTH_OPEN) ) {
            ESP_LOGV(TAG," ssid %s: encrypted but no password",(const char *)c->ssid);
            goto NEXT;
        }

#if 0 // skip this heuristic for now.... 
        // if I tried and failed, skip? unless maybe it's the only one left?
        if ( c->failed ) {
            print(" ssid %s: failed before, skipping\n",(const char *)c->ssid);
            break;
        }
#endif
        // make sure it's not stale
        if ( now > ( c->last_seen + WIFI_STALE_TIME ) ) {
//          not sure how to log int64_t ??? 
            ESP_LOGV(TAG, " ssid %s: seems stale, last seen %lld, now %lld skipping",
                (const char *)c->ssid,c->last_seen, now);
            goto NEXT;
        }

        // this is better than nothing!
        if (r == 0) {
            ESP_LOGV(TAG," ssid %s: selecting because nothing else",(const char *)c->ssid);
            r = c;
            goto NEXT;
        }

        ESP_LOGV(TAG," ssid best %s: suc %d fail %d rssi %d",(const char *)r->ssid,r->successes,r->fails,r->last_rssi);

        // Heuristic. Want fails to count for something, and successes
        int c_factor = (c->successes * 2) - c->fails;
        int r_factor = (r->successes * 2) - r->fails;
        if (c_factor > r_factor) {
            ESP_LOGV(TAG," ssid %s: selecting because factors: r: %d vs c: %d ",(const char *)c->ssid,r_factor,c_factor);
            r = c;
            goto NEXT;
        }   

        // now let's go for RSSI
        if (c->last_rssi > r->last_rssi) {
            ESP_LOGV(TAG," ssid %s: selecting because better rssi: %d vs %d ",(const char *)c->ssid,r->last_rssi,c->last_rssi);
            r = c;
            goto NEXT;
        }

NEXT:
        c = c->next;

    } /* end while */


    xSemaphoreGive(g_wifi_ap_info_mutex);

    return(r);

}


// This task attempts to connect to the current best if disconnected only.
// The list of passwords is taken from the registered set

void wifi_connect_task(void *pvParameters) {

    while(1)
    {
	    // What state am I in? if disconnected, start a connect
	    if ((g_is_connected == false) && (g_is_connecting == false)) {

            wifi_ap_info_t *ap = wifi_multi_find_best();
            if (ap) {

                wifi_config_t wifi_config = {
                    .sta = {
                        .scan_method = WIFI_ALL_CHANNEL_SCAN,
                        .sort_method = WIFI_CONNECT_AP_BY_SIGNAL,
                        .threshold.rssi = 0, // default?
                        .threshold.authmode = ap->authmode,
                    },
                };
                u8cpy(wifi_config.sta.ssid, ap->ssid);
                u8cpy(wifi_config.sta.password, ap->password);

                // this is how we set the SSID and password to use
                // note, the interface type being STA is not documented, it seems
                // when you set a config, you might end up cancelling a scan
                ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );

                ESP_LOGI(TAG, "CONNECT TO ap SSID:%s password:%s",
                         (const char *) ap->ssid, (const char *) ap->password);
                
                g_is_connecting = true;
                esp_err_t err = esp_wifi_connect();
                if (err != ESP_OK) {
                    g_is_connecting = false;
                    ESP_LOGW(TAG, " attempted to connect, couldn't, error %d",err);
                }
            }

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

    // beginning of new esp-idf network section....
    // please see 
    ESP_ERROR_CHECK( esp_netif_init() );

    // need somewhere to statically define mutex. Should be a better way, like how Linux
    // does it, so you don't have to put it in your init code
    // CREATE MUTEX STATIC - static allocations off by default, in the esp32
    // branch. Don't want to make everyone have to turn that on, so going
    // to use dynamic for now.
	g_wifi_ap_info_mutex = xSemaphoreCreateMutex( );

    g_wifi_scan_mutex = xSemaphoreCreateMutex();

    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // this is the new step in ESP-IDF 4.1++ required to tell the IP stack about the interface
    g_wifi_netif = esp_netif_create_default_wifi_sta();

    wifi_init_config_t wifi_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_config));

    // instance register will return the instance of the event handler you were registered with,
    // and allows you to deregister, because you can deregister from the correct event loop.
    // however, you can pass null if you don't intend to deregister, ie, only when the event
    // handler is removed, so null for the instance is fine
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    wifi_country_t wifi_country = {
        .cc = "US",
        .schan = 1,
        .nchan = 11,
    };

    // when plugged in ( not on battery ), why not use the normal power mode?
    // PS is POWER SAVE, WIFI_PS_MIN_MODEM wakes up every DTIM period to receive packets
    // WIFI_PS_MAX_MODEM , which listens to the station config about when to receive beacons
    // WIFI_PS_NONE just keeps the chip on all the time
    // ( not entirely sure if this gets wiped out by the set mode, hope not? )
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE) );

    // I am a client, mode STA
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );

    // this sets a lot of things we may not right at the beginning. We want to scan first and connect later,
    // so don't set the config of the AP you'd like to connect to.
    //ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_init_config) );
    ESP_ERROR_CHECK(esp_wifi_set_country(&wifi_country));

    ESP_ERROR_CHECK(esp_wifi_start() );

    // check here?
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE) );

    // kick off scan and connect tasks
    xTaskCreate(wifi_scan_task, "wifi_scan_task",4096/*stacksizewords*/, 
                (void *) NULL/*param*/, 5 /*pri*/, &g_xScanTask/*createdtask*/);

    xTaskCreate(wifi_connect_task, "connect",4096/*stacksizewords*/, 
                (void *) NULL/*param*/, 5 /*pri*/, &g_xConnectTask/*createdtask*/);

    ESP_LOGD(TAG, "wifi_init_multi finished.");

}

void wifi_multi_loglevel_set(esp_log_level_t level)
{

    esp_log_level_set(TAG, level);

}

