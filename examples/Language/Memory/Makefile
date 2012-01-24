all: memory

EDL=../../../bin/edl.exe

clean:
	$(RM) -rf out/*
	rmdir out
	$(RM) memory.exe

out/memory.lls: memory.edl
	mkdir -p out
	$(EDL) memory.edl >out/memory.lls

out/memory.lls.s: out/memory.lls
	llc out/memory.lls

memory: memory.c out/memory.lls.s
	gcc memory.c out/memory.lls.s -o memory.exe
