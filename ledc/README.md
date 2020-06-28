# LEDC!

This is a quick project using the tech / code I built up or FANC, but 
now including the LED controller. Which means I want a web page,
I want it continually using Wifi, and I want to see if I can
maintain smooth patterns in a modest LED array.


# hardware configuration

I used a ESP32 PICO D4 dev board. I find these the best of the crop as of 2020, in that they
have an excellent power system, reflash every time with perfect stability.

The pins are wired as follows. The 12v and GND of the fan are wired to the power of the system.
Ground is on the "outside" and power is on the "inside".

On the other side of the fan connector, the outer is the driver data. That wll be connected to 18
directly.

The inner is the tachometer sense, 19. This is is an "open drain" that should drain to 12v, however,
that requires three resistors. I found that draining to 3.3v ( which is on the ESP board ) through
a 12k resistor shows a pretty ugly waveform on the scope, but one the ESP seems to be able to deal with.

Thus, sense -> 19, but at the same time sense -> 12k -> 3.3v ( that's how open drains work ).

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


