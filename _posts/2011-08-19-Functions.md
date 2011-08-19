---
layout: post
author: Savoury SnaX
title: Handlers
---

 I have added FUNCTIONs + the INTERNAL modifier mentioned in the previous post. I also then proceeded to shrink the simple 8080 emulation file some more by using these new fangled things.

 I've had a quick play around with another arcade machine (ShuffleBoard) which ran on the same hardware as space invaders. A few changes to the invaders emulation was needed because the port interface is very different. Had to spend some time debugging the rom to see why it would infinite reboot, turned out to be one of the ports not returning a correct value. I've not pushed this code upstream yet, it was more a curiousity to see if the 8080 would handle it. 

 The next problem to tackle is multiple files, as has been discussed before, each file should be treated as a standalone black box (micro chip?). In order to link multiple files together I need to extend the parser to allow the interfaces to be usuable from within another file. For now I`ll probably go a similar route to C (having a header file).

