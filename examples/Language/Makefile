all: hello

EDL=../../bin/edl.exe

clean:
	$(RM) -rf out/*
	rmdir out
	$(RM) hello.exe

out/HelloWorld.lls: HelloWorld.edl
	mkdir -p out
	$(EDL) HelloWorld.edl >out/HelloWorld.lls

out/HelloWorld.lls.s: out/HelloWorld.lls
	llc out/HelloWorld.lls

hello: HelloWorld.c out/HelloWorld.lls.s
	gcc HelloWorld.c out/HelloWorld.lls.s -o hello.exe
