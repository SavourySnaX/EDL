all: c64

GLLIBS= -I../../../glfw-3/include -L../../../glfw-3/lib -lglfw -lglu32 -lopengl32
ALLIBS= -I/c/program\ files\ \(x86\)\OpenAL\ 1.1\ SDK\include /c/program\ files\ \(x86\)\OpenAL\ 1.1\ SDK\libs\Win32\Openal32.lib
EDL=../../bin/edl.exe

clean:
	$(RM) -rf out/*
	rmdir out
	$(RM) c64.exe

out/m6526_cia0.lls: ../../chips/Accurate/m6526.edl
	mkdir -p out
	$(EDL) -O2 -s CIA0_ ../../chips/Accurate/m6526.edl >out/m6526_cia0.lls

out/m6526_cia0.lls.s: out/m6526_cia0.lls
	llc out/m6526_cia0.lls

out/m6526_cia1.lls: ../../chips/Accurate/m6526.edl
	mkdir -p out
	$(EDL) -O2 -s CIA1_ ../../chips/Accurate/m6526.edl >out/m6526_cia1.lls

out/m6526_cia1.lls.s: out/m6526_cia1.lls
	llc out/m6526_cia1.lls

out/m6522_disk_via0.lls: ../../chips/Accurate/m6522.edl
	mkdir -p out
	$(EDL) -O2 -s DISK_VIA0_ ../../chips/Accurate/m6522.edl >out/m6522_disk_via0.lls

out/m6522_disk_via0.lls.s: out/m6522_disk_via0.lls
	llc out/m6522_disk_via0.lls

out/m6522_disk_via1.lls: ../../chips/Accurate/m6522.edl
	mkdir -p out
	$(EDL) -O2 -s DISK_VIA1_ ../../chips/Accurate/m6522.edl >out/m6522_disk_via1.lls

out/m6522_disk_via1.lls.s: out/m6522_disk_via1.lls
	llc out/m6522_disk_via1.lls

out/m6510_main.lls: ../../chips/Accurate/m6510.edl
	mkdir -p out
	$(EDL) -O2 -t -s MAIN_ ../../chips/Accurate/m6510.edl >out/m6510_main.lls

out/m6510_main.lls.s: out/m6510_main.lls
	llc out/m6510_main.lls

out/m6502_disk.lls: ../../chips/Accurate/m6502.edl
	mkdir -p out
	$(EDL) -O2 -t -s DISK_ ../../chips/Accurate/m6502.edl >out/m6502_disk.lls

out/m6502_disk.lls.s: out/m6502_disk.lls
	llc out/m6502_disk.lls

c64: c64.c 1541.c gui/debugger.c gui/debugger.h out/m6502_disk.lls.s out/m6510_main.lls.s out/m6526_cia0.lls.s out/m6526_cia1.lls.s out/m6522_disk_via0.lls.s out/m6522_disk_via1.lls.s
	gcc -O3 -g -Wl,-Map,c64.map c64.c gui/debugger.c 1541.c out/m6510_main.lls.s out/m6502_disk.lls.s out/m6526_cia0.lls.s out/m6526_cia1.lls.s out/m6522_disk_via0.lls.s out/m6522_disk_via1.lls.s $(ALLIBS) $(GLLIBS) -o c64.exe
