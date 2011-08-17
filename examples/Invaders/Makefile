all: invaders

GLLIBS= -I../../../glfw-3/include -L../../../glfw-3/lib -lglfw -lglu32 -lopengl32
EDL=../../bin/edl.exe

clean:
	$(RM) -rf out/*
	rmdir out
	$(RM) invaders.exe

out/i8080.lls: ../../chips/Accurate/i8080.edl
	mkdir -p out
	$(EDL) ../../chips/Accurate/i8080.edl >out/i8080.lls

out/i8080.lls.s: out/i8080.lls
	llc out/i8080.lls

invaders: invaders.c invadersDebug.c gui/debugger.c gui/debugger.h gui/font.h out/i8080.lls.s
	gcc -O3 invaders.c invadersDebug.c gui/debugger.c out/i8080.lls.s $(GLLIBS) -o invaders.exe
