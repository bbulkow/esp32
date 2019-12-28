# esp32
Repository with ESP32 projects, in progress, but the ESP-IDF really likes projects that are in 
github

## Why are esp32's cool?

As of 2019, the ESP32 is a high MHZ, dual core system with Wifi and Bluetooth LE that costs
ten dollars ( available at Mouser and Digikey at that price for the "dev boards", and also
different ESP systems are available at AdaFruit and Sparkfun for more like $25).

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

Other embedded systems

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
the maintainer replied nearly immediately and you'll probably have a different problem.

It seems that python2 is required. ESP-IDF doesn't seem to work with Python3, even though py2 will go EOL.

If you use a python environment system ( I use pyenv ), you'll need to be careful about
installing python libraries. I decided to change 'pyenv' back to 'system', but it is
better to install the python libraries required using `pip` [ todo, put in better instructions ]

The instructions talk about putting esp-idf's 'export.sh' into your bash profile.
I wouldn't recommend that, because it takes about 2 to 3 seconds to parse in.

It is probably best to set the IDF environment and not use things in the home directory.
[ TODO: how to do that ]

## Serial ports

Shockingly, the serial port worked great! Although the instructions online weren't right.

First, do the typical action by finding the COM port by opening your windows device manager.

Second, have your ESP32 attached and use something like PUTTY to make sure all the connection is good.
If you reboot, then connect, you'll see the initial statments of the ESP32. It won't be listening, but you'll know your serial port works with Windows.

Then, you need to close that program ( there is a friendly error message ), and use linux
tools. I tried 'cu' and that didn't work, but the idf.py monitor did work. See below for
getting idf running.

The official instructions talk about chmod /dev/ttyS? to 666. I did this but am not sure it
made a difference.

The name of the device is /dev/ttyS? , where `?` is the windows number. You can't just look at the devs that "exist" because all of them are pre-created.

```
idf.py -p /dev/ttyS7 monitor
```

## Use cheat sheets

```
cd $D/esp/esp-idf
. ./export.sh
```

After this point, everything is idf.py .

## Project structure in IDF

NTS: turn on ccache

-- IDF_TARGET not set, using default target: esp32

IDF_PATH
IDF_PYTHON_ENV_PATH

Executing action: menuconfig                                                                                                                                                                                                      Running cmake in directory /mnt/c/Users/bbulk/dev/esp/esp32/hello_world/build                                                                                                                                                     Executing "cmake -G 'Unix Makefiles' -DPYTHON_DEPS_CHECKED=1 -DESP_PLATFORM=1 --warn-uninitialized -DCCACHE_ENABLE=0 /mnt/c/Users/bbulk/dev/esp/esp32/hello_world"...                                                             Warn about uninitialized values.                                                                                                                                                                                                  -- Found Git: /usr/bin/git (found version "2.17.1")     