---
layout: post
author: Savoury SnaX
title: Documentation
---

 First up, I've now posted a first draft documentation for the language. Its rough around the edges, and incomplete (as is the language), however should serve as a starting point for interested parties. The documentation describes how some of the high level components of the 8080 are emulated.

 At present the space invaders emulator is 66% EDL and 34% C. The C code is used to handle memory access, fire interrupts and process the screen updates. The screen update code is stupidly simple so I won't bother describing it here, suffice to say that when it is moved into EDL it will be slightly more interesting (and I do mean slightly!).

 The memory handler is somewhat more interesting however. Unlike traditional emulators where a function GetByte and SetByte would handle memory access, the same isn't quite true for the EDL version of the 8080. The memory handler has to watch the cpu pins and react to requests for memory based on what the pins are doing. On the 8080 whenever the processor executes a new machine cycle, it outputs a special signal on the data bus to let external hardware know what its doing.

 You may know that a processor takes a number of cpu clocks to actually execute an instruction. On the 8080 this is a minimum of 4 clocks (or T States) and a maximum of 17 clocks. NOP for instance consists of 1 machine cycle (Fetch) with a length of 4 T States, where as POP PSW consists of 3 machine cycles (Fetch,Stack Read & Stack Read) with a length of 10 T States. So for each machine cycle a status byte is output in the first T state of that cycle.

 The memory controller watches for this status word, and combined with the address provided by the address bus determines what memory is being accessed. The status word is actually a series of flags, although at present the memory controller looks for exact values e.g. SYNC_FETCH.

 The design of the 8080 meant it was possible to have the stack,io ports and memory as completely seperate units. I have no idea if all of these features were ever used, but it would effectively give you 64k of memory + a 64k stack + 256 bytes of input/output.

 Refering to the logic analyser I showed in a previous post :

 ![Logic Analyser](/EDL/images/timing.png)

 You should be able to see the sync pulses occurring quite clearly, and if you stare hard enough, probably work out the type of sync from the values on the data bus.

   
