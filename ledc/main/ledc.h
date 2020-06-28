/*
 * FANC.h
 * Fan controler, but more importantly, my first ESP32
 * project that controls something with a web interface and 
 * just a basic thing.
 *
 * AS-IS
 * Copywrite 2020, Brian Bulkowski
 *
 */

esp_err_t ledc_init(void);
void ledc_destroy(void);

esp_err_t webserver_init(void);
void webserver_destroy();

