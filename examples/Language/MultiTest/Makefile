all: multi

EDL=../../../bin/edl.exe

clean:
	$(RM) -rf out/*
	rmdir out
	$(RM) multi.exe

out/MultiTest.lls: MultiTest.edl
	mkdir -p out
	$(EDL) MultiTest.edl >out/MultiTest.lls

out/MultiTest.lls.s: out/MultiTest.lls
	llc out/MultiTest.lls

multi: main.c out/MultiTest.lls.s
	gcc main.c out/MultiTest.lls.s -o multi.exe
