# LEDC!

This is a quick project using the tech / code I built up or FANC, but 
now including the LED controller. Which means I want a web page,
I want it continually using Wifi, and I want to see if I can
maintain smooth patterns in a modest LED array.


# hardware configuration

I used a ESP32 PICO D4 dev board. I find these the best of the crop as of 2020, in that they
have an excellent power system, reflash every time with perfect stability.

GPIO 13 is wired to the LED string, through a level shifter.

# Wifi

This uses the wifimulti module I wrote. It is configured to use three of the wifi
networks I have around my house, and I have ( mostly ) not checked in the passwords.

Please replace the information in fanc_main.cpp with wifi that you tend to use.

A nice enhancement would be to use the wifi system with NVS, but I haven't gotten that far yet.

# Configure the project

There are a few settings you might need for your board, I've set up my favorites.

```
idf.py menuconfig
```

### Build and Flash

Build the project and flash it to the board, then run monitor tool to view serial output:

```
idf.py -p PORT flash monitor
```

(To exit the serial monitor, type ``Ctrl-]``.)


