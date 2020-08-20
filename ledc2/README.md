# LEDC2!

This is a quick project using the tech / code I built up or FANC, but 
now including the LED controller. Which means I want a web page,
I want it continually using Wifi, and I want to see if I can
maintain smooth patterns in a modest LED array.

# Testing for network and no flashing

A common problem of the ESP32 with LED control is that when flash is accessed,
it holds of interrupts that also use flash, which means, any execution
from executable flash _or_ the use of any static ( which are automatically put in flash ).

Since the FANC / LEDC code all uses network requests sourced out of flash,
as many systems will, there is the chance of generating a hold-off.

This test is to try different simple ways of accessing and using the RMT
driver so that you don't get flashes. There are two types of variants.

First is which interrupt handler to use. Most of the drivers use their own, not the 
supplied drivers, which means if you have your own ISR you have to make sure it's all
in flash and requesting high priority.

Second is whether to have on-the-fly translation. The RMT bit pattern is not the same as the
waveform, obviously, partially because of the nature of the 1-wire encoding, partially
because of the difference in hertz between what the RMT is being driven, and partially
there's an extra bit out of every 16 that's for another purpose.

In the most recent version of the RMT driver from Espressif, there's the ability to have a
translator function, so you don't end up blowing your memory. Or you can pre-build the 
RMT bit pattern, which means more stability in sending.

These options are open to the FastLED and NeoPixelBus systems as well.

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


