all: test

EDL=../../bin/edl.exe

clean:
	$(RM) -rf out/*
	rmdir out
	$(RM) test.exe
	$(RM) test.result

out/mb14241.lls: ../../chips/Accurate/mb14241.edl
	mkdir -p out
	$(EDL) -O2 ../../chips/Accurate/mb14241.edl >out/mb14241.lls

out/mb14241.lls.s: out/mb14241.lls
	llc out/mb14241.lls

test: test.c out/mb14241.lls.s
	gcc -g test.c out/mb14241.lls.s -o test.exe
	./test.exe | tee test.result
