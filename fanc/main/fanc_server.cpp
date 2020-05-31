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

#include <array>
#include <string>
#include <iostream>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "nvs_flash.h"

#include "esp_http_server.h"
#include "esp_err.h"

#include "fanc.h"

//
// I have used 'embed files' in the build system to include a small test jpeg

extern const uint8_t server_cheese_jpg_start[] asm("_binary_cheese_jpg_start");
extern const uint8_t server_cheese_jpg_end[] asm("_binary_cheese_jpg_end");

typedef struct static_content_s {
    const char *name;
    const char *content_type;
    const uint8_t *buf;
    ssize_t  buf_len;
} static_content_t;

// C doesn't in any way have the ability yet to iterate
// over a defined list without having the size of the list.
// Yet, let's give the modern world a try

static std::array<static_content_t, 1> static_contents {
    {
    "cheese.jpg", "image/png", 
        server_cheese_jpg_start, ( server_cheese_jpg_end - server_cheese_jpg_start)
    }
};


esp_err_t static_uri_handler(httpd_req_t *req) {

    esp_err_t err;

    printf(" received static URI request for %s\n",req->uri);

    size_t query_len = httpd_req_get_url_query_len(req);
    char *query = NULL;
    if (query_len > 0) {
        query_len++; // doc says +1 for the terminating zero
        query = (char *) malloc(query_len);

        err = httpd_req_get_url_query_str(req, query, query_len);
        if (err) { 
            printf(" could not get query string even with correct len %d %s\n",
                        err,esp_err_to_name(err));
            free(query);
            return(ESP_FAIL);
        }
        printf(" static: query string %s\n",query);
    }

    if (query) {
        // these are all static compiled in items
        // todo: have a nice little table
        bool found = false;
        for (auto& static_content : static_contents) {
            if (strcmp(static_content.name, query) == 0) {
                printf(" sending %s response\n",query);
                httpd_resp_set_type(req, static_content.content_type);
                httpd_resp_send(req, (const char *) static_content.buf, static_content.buf_len);
                found = true;
                break;
            }
        }
        if ( found == false) {
            printf(" query not found, sending 404\n");
            httpd_resp_send_404(req);
        }
        free(query);
    }
    // todo: return an error on unknown queries
    else {
        // default
        httpd_resp_sendstr(req, "Hello World static Response");
    }

    return( ESP_OK );
}

esp_err_t root_uri_handler(httpd_req_t *req) {

    printf(" received root URI request for %s\n",req->uri);

    const char resp[] = "Hello World Root Response";
    httpd_resp_send(req, resp, strlen(resp));

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

static httpd_handle_t g_httpserver = NULL;

esp_err_t webserver_init(void) {

    esp_err_t err;

    printf(" webserver init!!!\n");


    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    // there's a globaluserctx that gets passed on responses


    err = httpd_start( &g_httpserver, &config );
    if (err != ESP_OK) {
        printf(" could not start http server, error %d %s\n",err,esp_err_to_name(err));
        return(ESP_FAIL);
    }

    // static pages URI
    err = httpd_register_uri_handler(g_httpserver, &uri_static);
    if (err != ESP_OK) printf(" could not register static handler %d %s\n",err,esp_err_to_name(err));

    // dynamic index URI
    err = httpd_register_uri_handler(g_httpserver, &uri_root);
    if (err != ESP_OK) printf(" could not register root handler %d %s\n",err,esp_err_to_name(err));

    printf(" webserver init success!!!!\n");


    return(ESP_OK);
}

void webserver_destroy(void) {

    httpd_stop(g_httpserver);

}

