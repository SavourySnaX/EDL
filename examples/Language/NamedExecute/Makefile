all: named

EDL=../../../bin/edl.exe

clean:
	$(RM) -rf out/*
	rmdir out
	$(RM) named.exe

out/named.lls: named.edl
	mkdir -p out
	$(EDL) named.edl >out/named.lls

out/named.lls.s: out/named.lls
	llc out/named.lls

named: main.c out/named.lls.s
	gcc main.c out/named.lls.s -o named.exe
