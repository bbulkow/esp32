# esp32
Repository with ESP32 projects, in progress, but the ESP-IDF really likes projects that are in 
github

## Why are esp32's cool?

As of 2019, the ESP32 is a high MHZ, dual core system with Wifi and Bluetooth LE that costs
ten dollars ( available at Mouser and Digikey at that price for the "dev boards", and also
different ESP systems are available at AdaFruit and Sparkfun for more like $25). ESP32
systems similar to the dev boards are available on AliExpress for $4.

If you need to go cheaper, and you have the ESP8266. It has half the cores, half the processors,
and is closer to $2 ( aliexpress ) and $4.

The ESP32 and ESP8266 differ greatly in development environments.

## current embedded for art development choices

If you are running an ESP32, you can run pure arduino, but besides having an unpleasant
editor, arduino is built around a simpler programming model - more of a pure "old school"
embedded system with a setup() and loop() function. If you wish to do more than one 
action, you have to do old school things like have shared addresses and variables.

Classic ATmega systems, at this point, look quite dated and expensive compared to 
the ESP systems. The idea that you'd spend $25 to $35 for a system with only 16Mhz and 2Kb
and no built-in network just seems unnecessary.

The hottest thing in 2019 that's not the ESP32 are two different flavors of ARM
processors. The Sparkfun Artemis is ARM based, supports all the Arduino, and has
the kind of performance and RAM footprint of an ESP32 - all for $17 each. The Artemis
is a unique concept in that Sparkfun has orchtestrated the core module to also be
capable of high scale production use. In the other corner, we have the latest Adafruit
Feather boards, also ARM based, supporting Arduino.

However, neither of these boards support Wifi or Ethernet in a sensible way, nor
do they support OTA ( over the air updates ). Having a true MQTT stack that runs over
wireless is getting to be cooler and cooler, for simply plug-and-go use.

## ESP32 vs ESP8266

For a true single-action device, that can fit in the Arduino model, the ESP8266 is quite
an amazing choice - but has serious drawbacks.

The ESP8266 Wifi system is an excellent example. If you have a "blink" program, you'll see it pause
for 3 to 4 seconds while the Wifi code gets itself organized. Instead of having a 'yeild' call,
or requiring chunks of wifi to be done at different times ( which is required embedded system methods ),
the includes ESP8266 wifi code can block the entire system for a few seconds.

Imagine if you were writing an embedded system for a car, and it just blocked for two seconds!

As an old embedded systems guy, I am also horrified by the Arduino 'String' data structure.
The fact that it might allocate dynamic memory, and might do so in a very poor pattern, is 
easy to abuse. While you might not use 'String' yourself, the fact that it's there - and friendly - 
means your libraries might. For example, it seems the Wifi library uses it substantially. While
it looks like they're using it corretly ( don't grow strings ), it's easy to abuse.

## ESP32 and FreeRTOS

Usually, when you're programming an embedded system, you start building some of the core primitives to
make low latency systems easier to program. There are only a few basic capabilities: tasks,
concurrency control ( mutexes, semaphores, condvars ), and dynamic memory based on pools
to limit fragmentation.

FreeRTOS seems have them, and they're really good looking and basic.

Even more interestingly, FreeRTOS allows Arduino libraries and headers.

## Lua? Really?

Yes, there is a Lua development environment. It's called NodeMCU, and 
it maintained. It runs on top of the ESP-IDF toolchain.

## Python? That's more like it

Micropython seems well supported on the ESP32. Check out the micropython docs.

https://docs.micropython.org/en/latest/esp32/tutorial/intro.html

Nicely, this supports the neopixel driver - would be nicer if it was just
an LED driver that drove all sorts of things like FastLED.

Not trying this.

# ESP32, ESP-IDF, and WSL

## WSL is the awesome!

I tell people "windows is the new macintosh" and they don't believe me yet.

I say this because there's a broader range of better hardware, a more modern and better
operating system, and a better command line.

The idea that you've got a "BSD derived" environment, which also can't accept visual interface
based programs, and has only one form of package manager called "homebrew" which is maintained by some
guy, just isn't cool

What you want is some of the real linux environments ( debian and RHEL dervived ), support lots of
different distributions, and do so natively ( no virtual machines ), is exactly what you should have,
and WSL delivers. At this point, there's even WSL and WSL2, where WSL is written to WNT system calls,
and WSL2 uses virtualization technology, which speeds up access to Linux file systems.

I prefer WSL because the whole point is running all the command lines over a single file system.
Since WSL2 is optimized for the Linux file system which Windows can't read, that not good.
WSL2 is really just another virtualization system, and I already have a more compatible and better
one called Docker, or one that does Graphics called VMWare.

# WSL and ESP-IDF ( 2019 )

Here is my cookbook of all the things I ran into trying to set up ESP-IDF with WSL. I think it's more
comprehensive than other guides I've read.

## Installation notes

Environment: Ubuntu 18 WSL, fully patched. Windows 10 with all updates ( slow ring ) as of Dec 2019.

Almost certainly, you will want to install on NTFS, so you can use your favorite windows editors
and dev system. Mine is Sublime 3. 

I wasn't able to build from source. The problem I ran into was `stat` being picky, but
realistically this isn't a combo the maintainers use, so you might run into another problem. Nicely,
the maintainer replied nearly immediately and will likely have a fix in a few weeks, however, it's clear that WSL is not yet popular enough to have good maintance ( it happens ). You'll probably have a different problem.

It seems that python2 is required. ESP-IDF doesn't seem to work with Python3, even though py2 will go EOL.

If you use a python environment system ( I use pyenv ), you'll need to be careful about
installing python libraries. I decided to change 'pyenv' back to 'system', but it is
better to install the python libraries required using `pip` [ todo, put in better instructions ]. 

If you don't set IDF_TOOLS_PATH , there will be a $HOME/.espressif directory created
when ./install.sh is run. In WSL, you might want to think about the location, because
WSL and WSL2 has very different ideas about file system performance. I preferred to pull
out these directories into NTFS.

```
export IDF_TOOLS_PATH="$D/esp/espressif"   
```

The instructions talk about putting esp-idf's 'export.sh' into your bash profile.
I wouldn't recommend that, because it takes about 2 to 3 seconds to parse in. Having every
command line to this extra part even when I'm not working on my ESP32 project is a bit
much.

I have set the following environment variables. 

```
export IDF_TOOLS_PATH="$D/esp/espressif"
export IDF_PATH="$D/esp/esp-idf"
export IDF_TARGET="esp32"
```

## Serial ports

Shockingly, the serial port worked great! Although the instructions online weren't right, and I didn't succeed in getting rates above 115200

First, do the typical action by finding the COM port of your board by opening your windows device manager. COM port seems to change based on both the hardware port on your computer, and the physical board you're using.

Second, have your ESP32 attached and use something like PUTTY to make sure all the connection is good.
If you reboot, then connect, you'll see the initial statments of the ESP32 bootloader. It won't be listening, but you'll know your serial port works with Windows.

Then, you need to close that program ( there is a friendly error message about windows having the device open if you forget ), and use linux
tools. I tried 'cu' and that didn't work, but the idf.py monitor did work. See below for
getting idf running.

The name of the device is /dev/ttyS? , where `?` is the windows number. You can't just look at the devs that "exist" because all of them are pre-created.

The official instructions talk about chmod /dev/ttyS? to 666. I did this but am not sure it
made a difference.


```
idf.py -p /dev/ttyS7 monitor
```

## Pick your fork!

When using ESP-IDF, please remember that the master you clone is the dangerous bleeding
edge. I don't yet know how dangerous, but I'm not itching to find out. When you fetch
ESP-IDF, you almost certainly want to choose a branch. As of today, that branch is
almost certainly the 4.0 release branch or the 3.3 release branch. There are apparently
incompatibilities, and code that you write ( or reuse ) should be clear about whether
its written for 3.x or 4.x.

See the above section about setting the IDF_TOOLS_PATH, because if you want a non-standard location for the idf-tools you'll need to change that before the install and have it set
as an environment variable during install.

```
git clone --recursive --branch release/4.0 https://github.com/espressif/esp-idf
cd esp-idf
./install.sh
```

or

```
git clone --recursive --branch release/3.3 https://github.com/espressif/esp-idf 
cd esp-idf
./install.sh
```

## Use cheat sheets

```
cd $D/esp/esp-idf
. ./export.sh
```

After this point, everything is idf.py .

## Project structure in IDF

# ESP32's in the wild

## reminder blink pin

The blink pin is set in menuconfig at the top level under 'example configuration'

## DevKitV1

This is fairly popular. It has a 240Mhz clock speed, uses DIO like most boards,
and the standard crystal speed. It's pinouts can be found through this tutorial
https://randomnerdtutorials.com/esp32-pinout-reference-gpios/ .

I got these from Amazon at a price of $7 ( $14 for two). They're all over
aliexpress as low as $4.

Notably, the Blink pin is 2, so your first tutorial works. This is changed
under menuconfig.

In order to flash this board, you'll need to hold down the 'boot' while
running the Flash program until it succeeds.

## DevKitV4

These are sold in the US by Mouser and Digikey. They come with a number of pin
formats, and parts with a `-F` are "female", which appears to be the new format.
I have these on order.

## SparkFun Thing

This device needs the XTAL clock set to 26 ( or autodetect ), which is not standard.

It does not require pressing pins to reflash! OMG!

Otherwise, it's a pretty standard looking WROOM32, with DIO and 4MB and whatnot.

## The elusive LOLIN D32 Mini

The most intersting form factor I've seen is an ESP32 form factor ( the short one ),
but with two rows of pins on each side for the extra GPIO devices. It appears that
Lolin has discontinued this board, so either it was unpopular due to form factor,
or unpopular because there were particular signal and/or reliability issues.

## What about ethernet?

I have found three ESP32 boards with on-board ethernet. One is the Espressif ESP32-Ethernet-Kit V1.1, which is available at Mouser and Digikey at ( gulp ) $50,
and the wESP ( wired esp ) at https://wesp32.com , and the Olimex board
for about $20 https://www.olimex.com/Products/IoT/ESP32/ESP32-POE/open-source-hardware .
The Olimex has scary statements about how to power, or not power the board
and the amount of isolation they have between Ethernet and USB.

## NodeMCU

THis was a popular ESP8266 development board form factor. Although there
are some signs of an ESP32 variant, it appears they're really discussing
using the NodeMCU software stack https://nodemcu.readthedocs.io/en/dev-esp32/ on
top of one of the other ESP32 boards.