all: vic20

GLLIBS= -I../../../glfw-3/include -L../../../glfw-3/lib -lglfw -lglu32 -lopengl32
EDL=../../bin/edl.exe

clean:
	$(RM) -rf out/*
	rmdir out
	$(RM) vic20.exe

out/m6502.lls: ../../chips/Accurate/m6502.edl
	mkdir -p out
	$(EDL) ../../chips/Accurate/m6502.edl >out/m6502.lls

out/m6502.lls.s: out/m6502.lls
	llc out/m6502.lls

vic20: vic20.c out/m6502.lls.s
	gcc -O3 vic20.c out/m6502.lls.s $(GLLIBS) -o vic20.exe
