all: spectrum spectrumSimple

GLLIBS= -lglfw3 -lglu32 -lopengl32 -lgdi32
ALLIBS= -lOpenAL
EDL=../../bin/edl.exe

clean:
	$(RM) -rf out/*
	rmdir out
	$(RM) spectrum.exe
	$(RM) spectrumSimple.exe

out/z80.lls: ../../chips/Accurate/z80.edl
	mkdir -p out
	$(EDL) -O2 ../../chips/Accurate/z80.edl >out/z80.lls

out/z80s.lls: ../../chips/Simplified/z80.edl
	mkdir -p out
	$(EDL) -O2 ../../chips/Simplified/z80.edl >out/z80s.lls

out/z80.lls.s: out/z80.lls
	llc out/z80.lls

out/z80s.lls.s: out/z80s.lls
	llc out/z80s.lls

spectrum: cpupin.c common.c out/z80.lls.s 
	gcc -O3 -g -pg cpupin.c common.c out/z80.lls.s $(ALLIBS) $(GLLIBS) -o spectrum.exe

spectrumSimple: cpusimple.c common.c out/z80s.lls.s 
	gcc -O3 -g -pg cpusimple.c common.c out/z80s.lls.s $(ALLIBS) $(GLLIBS) -o spectrumSimple.exe

