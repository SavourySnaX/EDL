all: spectrum

GLLIBS= -I../../../glfw-3/include -L../../../glfw-3/lib -lglfw -lglu32 -lopengl32
ALLIBS= -I/c/program\ files\ \(x86\)\OpenAL\ 1.1\ SDK\include /c/program\ files\ \(x86\)\OpenAL\ 1.1\ SDK\libs\Win32\Openal32.lib
EDL=../../bin/edl.exe

clean:
	$(RM) -rf out/*
	rmdir out
	$(RM) spectrum.exe

out/z80.lls: ../../chips/Accurate/z80.edl
	mkdir -p out
	$(EDL) ../../chips/Accurate/z80.edl >out/z80.lls

out/z80.lls.s: out/z80.lls
	llc out/z80.lls

spectrum: spectrum.c out/z80.lls.s 
	gcc -O3 -g spectrum.c out/z80.lls.s $(ALLIBS) $(GLLIBS) -o spectrum.exe
