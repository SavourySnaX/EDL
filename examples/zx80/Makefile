all: zx80

GLINCS= -I../../../glfw-3.0.4/include 
GLLIBS= -L../../../glfw-3.0.4/lib-mingw -lglfw3 -lglu32 -lopengl32 -lgdi32
ALINCS= -I/c/program\ files\ \(x86\)\OpenAL\ 1.1\ SDK\include 
ALLIBS= /c/program\ files\ \(x86\)\OpenAL\ 1.1\ SDK\libs\Win32\Openal32.lib
ZINCS= -I../../../zlib125 
ZLIB= -lz
EDL=../../bin/edl.exe

PROF_FLAGS=

clean:
	$(RM) -rf out/*
	rmdir out
	$(RM) zx80.exe

out/z80.lls:	../../chips/Accurate/z80.edl
	mkdir -p out
	$(EDL) -O2 ../../chips/Accurate/z80.edl >out/z80.lls

out/z80.lls.s: out/z80.lls
	llc -O3 out/z80.lls

out/zx80.o: zx80.c
	mkdir -p out
	gcc -O3 -c zx80.c $(GLINCS) $(ALINCS) $(ZINCS) -o out/zx80.o

zx80: out/zx80.o out/z80.lls.s
	gcc -g3 -O3 $(PROF_FLAGS) -Wl,-Map,zx80.map out/zx80.o out/z80.lls.s $(ALLIBS) $(GLLIBS) $(ZLIB) -o zx80.exe
