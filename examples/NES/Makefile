all: nes

GLLIBS= -I../../../glfw-3/include -L../../../glfw-3/lib -lglfw -lglu32 -lopengl32
ALLIBS= -I/c/program\ files\ \(x86\)\OpenAL\ 1.1\ SDK\include /c/program\ files\ \(x86\)\OpenAL\ 1.1\ SDK\libs\Win32\Openal32.lib
EDL=../../bin/edl.exe

clean:
	$(RM) -rf out/*
	rmdir out
	$(RM) nes.exe

out/rp2a03.lls: ../../chips/Accurate/rp2a03.edl
	mkdir -p out
	$(EDL) -O2 -s MAIN_ ../../chips/Accurate/rp2a03.edl >out/rp2a03.lls

out/rp2a03.lls.s: out/rp2a03.lls
	llc out/rp2a03.lls

nes: nes.c gui/debugger.c gui/debugger.h out/rp2a03.lls.s
	gcc -O3 -g -Wl,-Map,nes.map nes.c gui/debugger.c out/rp2a03.lls.s $(ALLIBS) $(GLLIBS) -o nes.exe
