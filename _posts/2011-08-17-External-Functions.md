---
layout: post
author: Savoury SnaX
title: External Functions
---

 I've just added support to EDL for calling external C functions. This will allow me to proceed to the next stage of moving the Space Invaders memory and video handling into EDL itself. 

 In order to test the external calling interface, I've implemented a second version of the Intel 8080 in EDL. This one mirrors the standard emulator practice of having a single STEP function, which executes 1 instruction and gives a number of cycles count. This is obviously less accurate from a hardware point of view, however it is a much simpler introduction to EDL, and is obviously significantly quicker. ~750 lines of code in comparison to ~3000 of the pin accurate version.

 All seems to be working well, I had to fix up the issue with only having 1 EXECUTE command in the file. EDL now supports as many as you want. But they must still come before the definition of instructions.

 The language documentation has been updated to reflect the changes.
