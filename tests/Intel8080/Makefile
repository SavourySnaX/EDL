all: test testStep

EDL=../../bin/edl.exe

clean:
	$(RM) -rf out/*
	rmdir out
	$(RM) test.exe
	$(RM) test.result
	$(RM) testStep.exe
	$(RM) testStep.result

out/pin8080.lls: ../../chips/Accurate/i8080.edl
	mkdir -p out
	$(EDL) -O2 ../../chips/Accurate/i8080.edl >out/pin8080.lls

out/step8080.lls: ../../chips/Simplified/i8080.edl
	$(EDL) -O2 ../../chips/Simplified/i8080.edl >out/step8080.lls

out/pin8080.lls.s: out/pin8080.lls
	llc out/pin8080.lls

out/step8080.lls.s: out/step8080.lls
	llc out/step8080.lls

test: testHarness.c out/pin8080.lls.s
	gcc -g testHarness.c out/pin8080.lls.s -o test.exe
	./test.exe | tee test.result

testStep: testHarnessSimplified.c out/step8080.lls.s
	gcc -g testHarnessSimplified.c out/step8080.lls.s -o testStep.exe
	./testStep.exe | tee testStep.result

