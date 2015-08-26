all: test

EDL=../../bin/edl.exe

clean:
	$(RM) -rf out/*
	rmdir out
	$(RM) test.exe
	$(RM) test.result

out/states.lls: states.edl
	mkdir -p out
	$(EDL) states.edl >out/states.lls

out/states.lls.s: out/states.lls
	llc out/states.lls

test: test.c out/states.lls.s
	gcc -g test.c out/states.lls.s -o test.exe
	./test.exe | tee test.result
