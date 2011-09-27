all: vic20

GLLIBS= -I../../../glfw-3/include -L../../../glfw-3/lib -lglfw -lglu32 -lopengl32
ALLIBS= -I/c/program\ files\ \(x86\)\OpenAL\ 1.1\ SDK\include /c/program\ files\ \(x86\)\OpenAL\ 1.1\ SDK\libs\Win32\Openal32.lib
EDL=../../bin/edl.exe

clean:
	$(RM) -rf out/*
	rmdir out
	$(RM) vic20.exe

out/m6502.lls: ../../chips/Accurate/m6502.edl
	mkdir -p out
	$(EDL) -O2 ../../chips/Accurate/m6502.edl >out/m6502.lls

out/m6502.lls.s: out/m6502.lls
	llc out/m6502.lls

vic20: vic20.c out/m6502.lls.s
	gcc -O3 -g vic20.c out/m6502.lls.s $(ALLIBS) $(GLLIBS) -o vic20.exe
