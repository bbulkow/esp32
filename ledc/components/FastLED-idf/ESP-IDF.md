# Port to ESP-IDF

THis is based off the 3.3 version of FastLED. The 3.3 version is where a lot of development paused, although there are also
a lot of good small fixes. Note that in most cases, nothing in the root directory changes - the changes are primarily
in the platforms/esp/32 directory. There are primarily a series of small changes.

What I greatly dislike is the continued reliance on Arduino code here and there, such as "millis()". The concern is that
one uses other arduino-derived code, and there are clashes. It would be better to either have a single, centralized
arduino porting facility, or to root everything out of the code. But, we continue.

# Environment

This port is to be used with ESP-IDF version 4.x, which went GA on about Feb, 2020.

In a number of cases, the code can't easily be made to support all versions of ESP-IDF.

In more recent versions of ESP-IDF, there is a new `_ll_` interface for the RMT system. Other posters
have re-tooled to that interface instead of writing directly to hardware addresses.

# Differences

It is not clear that using the ESP-IDF interrupt handler works anymore, although it should be tried. With larger
memory buffers, and using the translate function, it should work no better or worse than any other LEVEL3 interrupt.

Recent RMT driver code also includes setting the "ownership" of the shared DRAM. THis was overlooked in the FastLED
driver code, but has been implemented. It seemed to make no difference to the stability of the system.

# Difficulties

## Timing and glitches

The greatest difficulty with controlling any WS8211 system with the ESP32 / ESP8266 RMT system is timing.

The WS8211 single wire protocols require two transitions to make a bit.

The definition of the RMT interface means you put a time between the transition ( in divided 80mhz intervals, fit into a 15 bit 
field with the high bit being the value to emit ). A single RMT buffer has 64 values, but we use a "double buffer" strategy
(recommended by documentation). This means that 32 values, with 32 bits each, requires re-feeding the buffer about every 35 us.
The buffer won't fully run dry until 70us, but at 35us.

Interupts can be run in C at "medium priority", which means that there are a class of activities - such as Wifi - which can 
create enough jitter to disturb 35us timing requirement. I have observed this with a very simple REST web service using
the ESP-IDF supplied web server.

The RMT interface allows using more buffering, which will overcome this latency. It's a tradeoff: if one isn't using wifi,
one can perhaps get away with the default "1" size of buffers. If one notices glitches, using the larger buffers might be
required. At that point, you can't use all channels at the same time: changing the buffer size to 2 gives 4 buffers, 3 gives three,
etc.

Running a slower or faster WS8211 frequency will also effect this math.

## Reproducing

This effect is also seen with the NeoPixelBus interface ported to ESP-IDF, and also the example code in led-control provided
by ESP-IDF. Thus the issue is not in the FastLED library, but simply a jitter issue in the RTOS. It seems that people
using Arduino do not use the same TCP/IP stack, and do not suffer from these issues. Almost certainly, they are running at
lower priorities or with a different interrupt structure.

A simple test, using 99% espressif code, would open a simple HTTP endpoint and rest service, connect to WIFI and maintain an IP address,
then use the existing WS8211 sample code provided by Espressif. I contend that a web server which is simply returning "404", and
is being hit by mutiple requests per second ( I used 4 windows with a refresh interval of 2 seconds, but with cached content ) will
exhibit this latency.

## Measuring glitches

There is a set of debug code in the RMT interfaces which will show the timings for your environment. These use a classic "memorybuffer"
strategy where at interrupt time the "print" is done to a ram buffer, then the application can scoop up and print ( when it's safe ). 

## Running at a higher priority

ESP-IDF has a page on running at higher prioity: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/hlinterrupts.html

This page says (somewhat incorrectly) that C code may not be run from high priority interrupts. The document then 
disagrees with itself, and the example presented calls C saying it's safe because the C code can be examined and has no
return issues (or something). The issue appears to be some form of stack
checking or interrupt handling that's inserted by the compiler or the linker.

I would greatly hope that ESP-IDF improves the documentation around higher priority interrupts, and about manipulating the interrupts
in the system. I'd like to be able to profile and find what's causing these long delays, and drop their priority a little.
The FreeRTOS documentation ( on which ESP-IDF is built ) clearly says that high priority interrupts are required for motor control,
and LED control falls into that category. Placing the unnecessary barrier of assembly language isn't a terrible thing,
but it's not the right way - show how to write safe C code and allow raising the priority, even if some people will
abuse the privledge.

Either the code can be hand-written in assembly, or the existing working function can be decompiled and used as a source.

# Todo - why the jitter?

The large glitches ( 50us and up ) at the highest possible prority level, even with 2 cores of 240Mhz, is almost implausible.

Possible causes might be linked to the Flash subsystem. However, disabling all flash reads and writes ( the SPI calls ) doesn't change the glitching behavior.

Increasing network traffic does. Thus, the Wifi system is implicated. Work to do would be to decrease, somehow, the priority of 
those interrupts. One forum poster said they found a way. As TCP and Wifi are meant to be unreliable, this would cause 
performance issues, and that would have to be weighed against the nature of the poor LED output.

# Todo - why visual artifacts?

As one can easily detect the jitter, it would seem that simply stopping the send would halt the rest of the poorly formed output.
However, even with protection code added and no extra data transferred to RMT, there are still visual glitches. This should
be investigated, because if the glitches were less visible, the risk would be OK that sometimes network traffic would cause
jitter.



