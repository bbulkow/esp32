/* FANC 

   Fan controller. Uses a very basic web plus rest system to do something basic.

   Will likely be my basic template for doing similar stuff in the future

   This file spawns a small web server. It will respond to some template
   files, and also respond to a REST endpoint for dynamic stuff,
   and also take posts to actually change things.

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <inttypes.h>

#include <array>
#include <string>
#include <iostream>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "nvs_flash.h"
#include "cJSON.h"

#include "esp_http_server.h"
#include "esp_err.h"

#include "esp_log.h"
static const char *TAG = "fanc";

#include "fanc.h"



// return the integer value of a json element which is in the root
// Extensions: get a subitem with cJSON_GetObjectItem(parentCJson,child-string)
//             set values cJSON_GetObjectItem(format,"frame rate")->valueint=25;
// deleting the root deletes all the things
// https://github.com/espressif/esp-idf/tree/master/components/json
// not sure what happens when we can't get the string, though
static esp_err_t get_json_value_int(const char *json_str, const char *field, int *val) {

    ESP_LOGV(TAG," getjsonvalueint: json %s field %s",json_str,field);
    const char *err_loc = 0;
    cJSON *root = cJSON_ParseWithOpts(json_str, &err_loc, false);
    ESP_LOGV(TAG," cjson parse: root is %p",root);
    if (! root) {
        ESP_LOGV(TAG,"could not parse json, error at postition: %d",(int)(err_loc - json_str) );
        return(ESP_FAIL);
    }

    cJSON *field_object = cJSON_GetObjectItem(root,field);
    if (! field_object) {
        ESP_LOGD(TAG, " JSON parse could not find %s in %s",field,json_str);
        cJSON_Delete(root);
        return(ESP_FAIL);
    }
    if (! cJSON_IsNumber(field_object)) {
        ESP_LOGD(TAG, " JSON did not find a number %s in %s",field,json_str);
        cJSON_Delete(root);
        return(ESP_FAIL);
    }

    // NB. This library has a strange idea that number is both double and integer.
    // in this case I want the closest int.

    *val = field_object->valueint;
    cJSON_Delete(root);
    return(ESP_OK);
}

// rest get calls will come here
//

esp_err_t rest_uri_handler(httpd_req_t *req) {

    ESP_LOGD(TAG," received rest URI request for %s method %d",req->uri,req->method);

    // did I get content?

    char *content = 0;
    if (req->content_len > 0) {
        //ESP_LOGD(TAG," content length is: %d",req->content_len);
        content =(char *)malloc(req->content_len + 1);
        int sz = httpd_req_recv(req, content, req->content_len);
        //ESP_LOGD(TAG," content sz is: %d",sz);
        if (sz != req->content_len) {
            //ESP_LOGD(TAG," could not retrieve content: %d",sz);
            httpd_resp_send_500(req);
            free(content);
            return(ESP_OK);
        }
        content[req->content_len] = 0;
        //ESP_LOGD(TAG," content is: %s",content);
    }
    
// going to ignore accept headers...
#if 0
    // what is the accept string?
    size_t hdr_value_len = httpd_req_get_hdr_value_len(req, "Accept");
    if (hdr_value_len) {
        char *value =(char *)malloc(hdr_value_len+1);
        httpd_req_get_hdr_value_str(req, "Accept", value, hdr_value_len );
        ESP_LOGD(TAG," got accept header: %s",value);
        free(value);
    }

    // what is the Connection string?
    size_t hdr_value_len = httpd_req_get_hdr_value_len(req, "Connection");
    if (hdr_value_len) {
        char *value =(char *)malloc(hdr_value_len+1);
        httpd_req_get_hdr_value_str(req, "Connection", value, hdr_value_len );
        ESP_LOGD(TAG," got connection header: %s",value);
        free(value);
    }
#endif

    // doing this up here will set it for everything
    httpd_resp_set_hdr(req,"Connection","close");

    // parse the URI so I can sub-dispatch?
    const char *last_slash = strrchr(req->uri, '/');
    last_slash++;

    if ( strcmp(last_slash, "fan_pct") == 0) {

        if (req->method == HTTP_GET) {

            char fan_pct_str[20];
            snprintf(fan_pct_str, sizeof(fan_pct_str),"%d",fanc_percentage_get() );

            ESP_LOGI(TAG,"rest: sending fan percentage %s",fan_pct_str);

            httpd_resp_set_type(req, "text/plain");
            httpd_resp_sendstr(req, fan_pct_str);
        }
        else if (req->method == HTTP_POST) {

            int new_fan_pct = 0;
            if (ESP_OK == get_json_value_int(content, "fan_pct",&new_fan_pct)) {
                ESP_LOGI(TAG,"rest: posted, a fan pct to set: %d",new_fan_pct);

                fanc_percentage_set(new_fan_pct);

                // easiest way to say OK?
                httpd_resp_sendstr(req,"");
            }
            else {
                ESP_LOGW(TAG," posted, a json with a bad value fan pct");
                httpd_resp_send_err(req,HTTPD_400_BAD_REQUEST,"illegal value");
            }
        }

    }
    else if ( strcmp(last_slash, "fan_speed") == 0) {

        if (req->method == HTTP_GET) {

            char fan_speed_str[20];
            snprintf(fan_speed_str, sizeof(fan_speed_str),"%.1f",fanc_speed_get() );

            ESP_LOGI(TAG,"rest: sending fan speed %s",fan_speed_str);

            httpd_resp_set_type(req, "text/plain");
            httpd_resp_sendstr(req, fan_speed_str);
        }
    }
    else if ( strcmp(last_slash, "uptime") == 0) {

        clock_t cl = clock();
        uint64_t uptime_sec = cl / CLOCKS_PER_SEC;
        char uptime_str[20];
        snprintf(uptime_str, sizeof(uptime_str),"%" PRIu64,uptime_sec);

        ESP_LOGI(TAG,"rest: sending uptime %s",uptime_str);

        httpd_resp_set_type(req, "text/plain");
        httpd_resp_sendstr(req, uptime_str);
    }
    else if ( strcmp(last_slash, "epoch") == 0) {

        // this might be correct, because we have sntp enabled
        time_t secs = time(NULL);
        char secs_str[30];
        // depending on config, time might be 64 bits, or 32.... play it safe
        snprintf(secs_str, sizeof(secs_str),"%" PRIu64,(uint64_t) secs);

        ESP_LOGI(TAG,"rest: sending epoch %s",secs_str);

        httpd_resp_set_type(req, "text/plain");
        httpd_resp_sendstr(req, secs_str);
    }

    else {
        ESP_LOGI(TAG,"rest: unknown endpoint: %s",last_slash);
        httpd_resp_send_404(req);
    }

    if (content) free(content);

    return(ESP_OK);
}


//
// I have used 'embed files' in the build system to include a small test jpeg

extern const uint8_t server_cheese_jpg_start[] asm("_binary_cheese_jpg_start");
extern const uint8_t server_cheese_jpg_end[] asm("_binary_cheese_jpg_end");

extern const uint8_t server_index_html_start[] asm("_binary_index_html_start");
extern const uint8_t server_index_html_end[] asm("_binary_index_html_end");

extern const uint8_t server_jquery_min_js_start[] asm("_binary_jquery_min_js_start");
extern const uint8_t server_jquery_min_js_end[] asm("_binary_jquery_min_js_end");


typedef struct static_content_s {
    const char *name;
    const char *content_type;
    const uint8_t *buf;
    ssize_t  buf_len;
} static_content_t;

// C doesn't in any way have the ability yet to iterate
// over a defined list without having the size of the list.
// Yet, let's give the modern world a try

// regarding extra braces: this requirement was removed at some point in the C++11 spec
// not sure why it's being required here...

// I'm going to mandate that "index.html", ie, what you want served out of the
// root, will always be first

static std::array<static_content_t, 3> static_contents = { {
    {
    "index.html", "text/html", 
        server_index_html_start, ( server_index_html_end - server_index_html_start)
    },    {
    "cheese.jpg", "image/jpg", 
        server_cheese_jpg_start, ( server_cheese_jpg_end - server_cheese_jpg_start)
    },
    {
    "jquery.min.js", "text/javascript", 
        server_jquery_min_js_start, ( server_jquery_min_js_end - server_jquery_min_js_start)
    }
} };


esp_err_t static_uri_handler(httpd_req_t *req) {

    esp_err_t err;

    ESP_LOGD(TAG," received static URI request for %s",req->uri);

    // short lived connections
    httpd_resp_set_hdr(req,"Connection","close");
    httpd_resp_set_hdr(req,"Cache-Control","max-age=99999");

    size_t query_len = httpd_req_get_url_query_len(req);
    char *query = NULL;
    if (query_len > 0) {
        query_len++; // doc says +1 for the terminating zero
        query = (char *) malloc(query_len);

        err = httpd_req_get_url_query_str(req, query, query_len);
        if (err) { 
            ESP_LOGI(TAG,"static_http: could not get query string even with correct len %d %s",
                        err,esp_err_to_name(err));
            free(query);
            return(ESP_FAIL);
        }
        ESP_LOGD(TAG," static: query string %s",query);
    }

    if (query) {
        // these are all static compiled in items
        // todo: have a nice little table
        bool found = false;
        for (auto& static_content : static_contents) {
            if (strcmp(static_content.name, query) == 0) {
                ESP_LOGD(TAG,"static_http: sending %s response",query);
                httpd_resp_set_type(req, static_content.content_type);
                httpd_resp_send(req, (const char *) static_content.buf, static_content.buf_len);
                found = true;
                break;
            }
        }
        if ( found == false) {
            ESP_LOGI(TAG,"static_http: query not found, sending 404");
            httpd_resp_send_404(req);
        }
        free(query);
    }
    // todo: return an error on unknown queries
    else {
        // default
        ESP_LOGI(TAG,"static_http: received no query, returing sill response");
        httpd_resp_sendstr(req, "static server expecting query string");
    }

    return( ESP_OK );
}

esp_err_t root_uri_handler(httpd_req_t *req) {

    ESP_LOGI(TAG," received root URI request for %s",req->uri);

    // short lived connections
    httpd_resp_set_hdr(req,"Connection","close");
    httpd_resp_set_hdr(req,"Cache-Control","max-age=99999");

    // by convention, whatever is 0 will be served out of root
    static_content_t *root = &static_contents[0];

    httpd_resp_set_type(req, root->content_type);
    httpd_resp_send(req, (const char *) root->buf, root->buf_len);

    return( ESP_OK );
}

httpd_uri_t uri_static {
    .uri = "/static",
    .method = HTTP_GET,
    .handler = static_uri_handler,
    .user_ctx = NULL
};

httpd_uri_t uri_root {
    .uri = "/",
    .method = HTTP_GET,
    .handler = root_uri_handler,
    .user_ctx = NULL
};

httpd_uri_t uri_rest_get {
    .uri = "/rest/*",
    .method = HTTP_GET,
    .handler = rest_uri_handler,
    .user_ctx = NULL
};

httpd_uri_t uri_rest_post {
    .uri = "/rest/*",
    .method = HTTP_POST,
    .handler = rest_uri_handler,
    .user_ctx = NULL
};


static httpd_handle_t g_httpserver = NULL;

esp_err_t webserver_init(void) {

    esp_err_t err;

    ESP_LOGI(TAG," webserver init!!!");


    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
     // this server does match wildcards if you use a fancier function
    config.uri_match_fn = httpd_uri_match_wildcard;
    // there's a globaluserctx that gets passed on responses


    err = httpd_start( &g_httpserver, &config );
    if (err != ESP_OK) {
        ESP_LOGW(TAG,"webserver_init: could not start http server, error %d %s",err,esp_err_to_name(err));
        return(ESP_FAIL);
    }

    // dynamic index URI
    err = httpd_register_uri_handler(g_httpserver, &uri_root);
    if (err != ESP_OK) { 
        ESP_LOGW(TAG, "webserver_init: could not register root handler %d %s",err,esp_err_to_name(err)); 
        return(ESP_FAIL);
    }

    // static pages URI
    err = httpd_register_uri_handler(g_httpserver, &uri_static);
    if (err != ESP_OK) { 
        ESP_LOGW(TAG, "webserver_init: could not register static handler %d %s",err,esp_err_to_name(err));         
        return(ESP_FAIL);
    }

    // rest get URI
    err = httpd_register_uri_handler(g_httpserver, &uri_rest_get);
    if (err != ESP_OK) { 
        ESP_LOGW(TAG, "webserver_init: could not register rest get handler %d %s",err,esp_err_to_name(err)); 
        return(ESP_FAIL);
    }

    // rest post URI
    err = httpd_register_uri_handler(g_httpserver, &uri_rest_post);
    if (err != ESP_OK) { 
        ESP_LOGW(TAG, "webserver_init: could not register rest post handler %d %s",err,esp_err_to_name(err)); 
        return(ESP_FAIL);
    }

    ESP_LOGI(TAG," webserver init success!!!!");

    return(ESP_OK);
}

void webserver_destroy(void) {

    httpd_stop(g_httpserver);

}

