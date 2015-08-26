all: test

EDL=../../bin/edl.exe

clean:
	$(RM) -rf out/*
	rmdir out
	$(RM) test.exe
	$(RM) test.result

out/74154.lls: ../../chips/Accurate/74154.edl
	mkdir -p out
	$(EDL) -O2 ../../chips/Accurate/74154.edl >out/74154.lls

out/74154.lls.s: out/74154.lls
	llc out/74154.lls

test: test.c out/74154.lls.s
	gcc -g test.c out/74154.lls.s -o test.exe
	./test.exe | tee test.result
