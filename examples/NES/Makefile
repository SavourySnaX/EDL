all: nes

GLLIBS= -I../../../glfw-3/include -L../../../glfw-3/lib -lglfw -lglu32 -lopengl32
ALLIBS= -I/c/program\ files\ \(x86\)\OpenAL\ 1.1\ SDK\include /c/program\ files\ \(x86\)\OpenAL\ 1.1\ SDK\libs\Win32\Openal32.lib
ZLIB= -I../../../zlib125 -lz
EDL=../../bin/edl.exe

PROF_FLAGS=#-pg -g3

clean:
	$(RM) -rf out/*
	rmdir out
	$(RM) nes.exe

out/rp2c02.lls:	../../chips/Accurate/rp2c02.edl
	mkdir -p out
	$(EDL) -O2 -s PPU_ ../../chips/Accurate/rp2c02.edl >out/rp2c02.lls

out/rp2c02.lls.s: out/rp2c02.lls
	llc -O3 out/rp2c02.lls

out/rp2a03.lls: ../../chips/Accurate/rp2a03.edl
	mkdir -p out
	$(EDL) -O2 -s MAIN_ ../../chips/Accurate/rp2a03.edl >out/rp2a03.lls

out/rp2a03.lls.s: out/rp2a03.lls
	llc -O3 out/rp2a03.lls

nes: nes.c gui/debugger.c gui/debugger.h out/rp2a03.lls.s minizip/miniunz.c minizip/iowin32.c minizip/ioapi.c minizip/unzip.c out/rp2c02.lls.s jake/ntscDecode.c jake/ntscDecode.h jake/ntscDecodeCrtsim.c jake/ntscDecodeCrtsim.h
	gcc -O3 $(PROF_FLAGS) -Wl,-Map,nes.map nes.c gui/debugger.c out/rp2a03.lls.s out/rp2c02.lls.s minizip/miniunz.c minizip/iowin32.c minizip/ioapi.c minizip/unzip.c jake/ntscDecode.c jake/ntscDecodeCrtsim.c $(ALLIBS) $(GLLIBS) $(ZLIB) -o nes.exe
