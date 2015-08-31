all: zx80

GLINCS= 
GLLIBS= -lglfw3 -lglu32 -lopengl32 -lgdi32
ALINCS= 
ALLIBS= -lOpenAL32
ZINCS= 
ZLIB= -lz
EDL=../../bin/edl.exe

PROF_FLAGS=

clean:
	$(RM) -rf out/*
	rmdir out
	$(RM) zx80.exe

out/cpu_rom_test.lls:	cpu_rom_test.eds chips/zx80_rom.edl ../../chips/Accurate/z80.edl
	mkdir -p out
	$(EDL) -O0 cpu_rom_test.eds >out/cpu_rom_test.lls

out/cpu_rom_test.lls.s: out/cpu_rom_test.lls
	llc -O0 out/cpu_rom_test.lls

out/zx80.o: zx80.c
	mkdir -p out
	gcc -g3 -O0 -c zx80.c $(GLINCS) $(ALINCS) $(ZINCS) -o out/zx80.o

zx80: out/zx80.o out/cpu_rom_test.lls.s
	gcc -g3 -O0 $(PROF_FLAGS) -Wl,-Map,zx80.map out/zx80.o out/cpu_rom_test.lls.s $(ALLIBS) $(GLLIBS) $(ZLIB) -o zx80.exe
