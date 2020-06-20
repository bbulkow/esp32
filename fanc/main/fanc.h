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

esp_err_t fanc_init(void);
void fanc_destroy(void);

int fanc_percentage_get(void);
esp_err_t fanc_percentage_set(int p);

// attempting to be revolutions per second.
float fanc_speed_get(void);

esp_err_t webserver_init(void);
void webserver_destroy();

