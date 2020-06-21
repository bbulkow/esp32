# FANC!

Oh, look, it's so FANC.

This is a PWM fan controller for a simple semi-industrial use. We need to dry
cheese, and we need to keep air flow, but we also need to keep flies away.
So, we have built a small box, and wanted a WIFI based controller.

This controller should allow setting the speed of the fan, but also
measure the speed ( since PWM fans will do that ).

PWM fans have four wires. Power and Ground which are 12v, and
a PWM speed setting one, and another "tachometer" pin which can
be read in order to see how fast the fan is spinning.

This standard was used for Intel CPU fans, which crept its way into other case
fans. These fans are pretty easy to come by.

# esp-idf

This project uses esp-idf. It does not use arduino. If you come across this project,
please go to Espressif's site and configure correctly and use the `idf.py build` correctly.

This is written for ESP-IDF 4.1 and better. That's because there were significant changes
in how the network is configured in 4.1, and instead of doing the hassle of back porting,
I thought I would just do both. This allows the newer S2 chips ( which require esp-idf v4.2 )
to work seamlessly, and those are a few bucks cheaper.

# Pin assignment

The pins on the fan are set in fanc.cpp. They are pin 18 for the control, and pin 19 for the
tachometer ( pulse counter ).

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


