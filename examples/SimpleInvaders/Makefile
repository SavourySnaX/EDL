all: invaders

GLLIBS= -lglfw3 -lglu32 -lopengl32 -lGDI32
EDL=../../bin/edl.exe

clean:
	$(RM) -rf out/*
	rmdir out
	$(RM) invaders.exe

out/i8080.lls: ../../chips/Simplified/i8080.edl
	mkdir -p out
	$(EDL) -O2 ../../chips/Simplified/i8080.edl >out/i8080.lls

out/i8080.lls.s: out/i8080.lls
	llc out/i8080.lls

out/mb14241.lls: ../../chips/Accurate/mb14241.edl
	$(EDL) ../../chips/Accurate/mb14241.edl >out/mb14241.lls

out/mb14241.lls.s: out/mb14241.lls
	llc out/mb14241.lls

invaders: invaders.c out/i8080.lls.s out/mb14241.lls.s
	gcc -O3 invaders.c out/i8080.lls.s out/mb14241.lls.s $(GLLIBS) -o invaders.exe
