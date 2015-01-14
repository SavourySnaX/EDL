all: test

EDL=../../bin/edl.exe

clean:
	$(RM) -rf out/*
	rmdir out
	$(RM) test.exe
	$(RM) test.result
	$(RM) simple.exe

out/test.lls: test.eds
	mkdir -p out
	$(EDL) -O3 test.eds >out/test.lls

out/test.lls.s: out/test.lls
	llc -O3 out/test.lls

test: test.c out/test.lls.s
	gcc -g test.c out/test.lls.s -o test.exe
	test.exe | tee test.result

out/simple.lls: simple.edl
	mkdir -p out
	$(EDL) -O3 simple.edl >out/simple.lls

out/simple.lls.s: out/simple.lls
	llc -O3 out/simple.lls


simple: simple.c out/simple.lls.s
	gcc -g simple.c out/simple.lls.s -o simple.exe
	simple.exe

