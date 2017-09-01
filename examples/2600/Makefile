all: 2600

GLLIBS= -I../../../glfw-3.0.4/include -L../../../glfw-3.0.4/lib-mingw -lglfw3 -lglu32 -lopengl32 -lgdi32
ALLIBS= -I/c/program\ files\ \(x86\)\OpenAL\ 1.1\ SDK\include /c/program\ files\ \(x86\)\OpenAL\ 1.1\ SDK\libs\Win32\Openal32.lib
EDL=../../bin/edl.exe

clean:
	$(RM) -rf out/*
	rmdir out
	$(RM) 2600.exe

# Cart types

out/2k.lls: carts/2k.edl
	mkdir -p out
	$(EDL) -O2 -s CART2K_ carts/2k.edl >out/2k.lls

out/2k.lls.s: out/2k.lls
	llc -O3 out/2k.lls

out/4k.lls: carts/4k.edl
	mkdir -p out
	$(EDL) -O2 -s CART4K_ carts/4k.edl >out/4k.lls

out/4k.lls.s: out/4k.lls
	llc -O3 out/4k.lls

out/8k.lls: carts/8k.edl
	mkdir -p out
	$(EDL) -O2 -s CART8K_ carts/8k.edl >out/8k.lls

out/8k.lls.s: out/8k.lls
	llc -O3 out/8k.lls

out/8kE0.lls: carts/8kE0.edl
	mkdir -p out
	$(EDL) -O2 -s CART8KE0_ carts/8kE0.edl >out/8kE0.lls

out/8kE0.lls.s: out/8kE0.lls
	llc -O3 out/8kE0.lls

out/12k.lls: carts/12k.edl
	mkdir -p out
	$(EDL) -O2 -s CART12K_ carts/12k.edl >out/12k.lls

out/12k.lls.s: out/12k.lls
	llc -O3 out/12k.lls

out/16k.lls: carts/16k.edl
	mkdir -p out
	$(EDL) -O2 -s CART16K_ carts/16k.edl >out/16k.lls

out/16k.lls.s: out/16k.lls
	llc -O3 out/16k.lls

out/16kSC.lls: carts/16kSC.edl
	mkdir -p out
	$(EDL) -O2 -s CART16KSC_ carts/16kSC.edl >out/16kSC.lls

out/16kSC.lls.s: out/16kSC.lls
	llc -O3 out/16kSC.lls

out/32k.lls: carts/32k.edl
	mkdir -p out
	$(EDL) -O2 -s CART32K_ carts/32k.edl >out/32k.lls

out/32k.lls.s: out/32k.lls
	llc -O3 out/32k.lls

out/32k3F.lls: carts/32k3F.edl
	mkdir -p out
	$(EDL) -O2 -s CART32K3F_ carts/32k3F.edl >out/32k3F.lls

out/32k3F.lls.s: out/32k3F.lls
	llc -O3 out/32k3F.lls

out/32kSC.lls: carts/32kSC.edl
	mkdir -p out
	$(EDL) -O2 -s CART32KSC_ carts/32kSC.edl >out/32kSC.lls

out/32kSC.lls.s: out/32kSC.lls
	llc -O3 out/32kSC.lls

# 2600

out/m6532.lls: ../../chips/Accurate/m6532.edl
	mkdir -p out
	$(EDL) -O2 -s RIOT_ ../../chips/Accurate/m6532.edl >out/m6532.lls

out/m6532.lls.s: out/m6532.lls
	llc -O3 out/m6532.lls

out/tia_ntsc.lls: ../../chips/Accurate/tia_ntsc.edl
	mkdir -p out
	$(EDL) -O2 -s TIA_ ../../chips/Accurate/tia_ntsc.edl >out/tia_ntsc.lls

out/tia_ntsc.lls.s: out/tia_ntsc.lls
	llc -O3 out/tia_ntsc.lls

out/m6507_main.lls: ../../chips/Accurate/m6507.edl
	mkdir -p out
	$(EDL) -O2 -s MAIN_ ../../chips/Accurate/m6507.edl >out/m6507_main.lls

out/m6507_main.lls.s: out/m6507_main.lls
	llc -O3 out/m6507_main.lls

2600: 2600.c CartIdent.c CartIdent.h out/m6507_main.lls.s out/tia_ntsc.lls.s out/m6532.lls.s out/2k.lls.s out/4k.lls.s out/8k.lls.s out/8kE0.lls.s out/12k.lls.s out/16k.lls.s out/16kSC.lls.s out/32k.lls.s out/32k3F.lls.s out/32kSC.lls.s
	gcc -O3 -g 2600.c CartIdent.c out/m6507_main.lls.s out/tia_ntsc.lls.s out/m6532.lls.s out/2k.lls.s out/4k.lls.s out/8k.lls.s out/8kE0.lls.s out/12k.lls.s out/16k.lls.s out/16kSC.lls.s out/32k.lls.s out/32K3F.lls.s out/32kSC.lls.s $(ALLIBS) $(GLLIBS) -o 2600.exe
