![CI](https://github.com/SavourySnaX/EDL/workflows/CI/badge.svg)

Emulation Description Language
------------------------------

Requires Bison/Yacc,lex/flex,llvm (11.0) to build the compiler.

Most of the examples require plus opengl and glfw (3.x).

If on windows you can use setupWin64.bat to clone and build the dependencies, otherwise use the platform package system to install the required dependencies.

I've now switched over to use cmake for development.

tests/ folder includes a simple test harness for validating the 8080 example cores work.

examples/ folder includes a few examples :)

```
	Invaders - Uses the pin accurate 8080 core, shows a logic analyser while running.
	Simple Invaders - Uses the step based 8080 core.
	Language - Contains a few small examples of constructs available in EDL.
	ZXSpectrum - Both a step based, and a pin accurate Z80 version.
	vic20 - Includes 1541 disk drive emulation + a lot of debug windows.
	c64 - Includes 1541 disk drive emulation -- Incomplete.
	NES - WIP (has a composite TV signal generator built in.. but output requires an external repository.)
	2600 - WIP
	zx80 - WIP (no display, currently just runs the rom)
	EDLSpectrum - Another spectrum emulator, but this time using bus connections etc.
```
