all: edl

OBJS = out/parser.o  \
       out/generator.o \
       out/main.o    \
       out/lexer.o  \

LLVM_MODULES = all

#TODO Fix so i can build in linux again
LLVM_CONFIG=llvm-config
START_GROUP=--start-group
END_GROUP=--end-group

CPPFLAGS = -g -Isrc -Iout `$(LLVM_CONFIG) --cppflags $(LLVM_MODULES)`
LDFLAGS = -g `$(LLVM_CONFIG) --ldflags $(LLVM_MODULES)`
LIBS = `$(LLVM_CONFIG) --libs $(LLVM_MODULES)` -lpthread -lpsapi -limagehlp

GLLIBS= -I../glfw-3/include -L../glfw-3/lib -lglfw -lglu32 -lopengl32

clean:
	$(RM) -rf out/*
	rmdir out
	$(RM) test.lls
	$(RM) test.lls.s
	$(RM) test.exe
	$(RM) test.result
	$(RM) invaders.exe
	$(RM) edl.exe

out/parser.cpp: src/edl.y 
	mkdir -p out
	bison -d -o $@ $^
	
out/parser.hpp: out/parser.cpp

out/lexer.cpp: src/edl.l out/parser.hpp
	flex -o $@ $^

out/%.o: src/%.cpp
	g++ -c $(CPPFLAGS) -o $@ $<


edl: $(OBJS)
	g++ -o $@ $(LDFLAGS) $(START_GROUP) $(OBJS) $(LIBS) $(END_GROUP)

test.lls: edl test.edl
	edl.exe test.edl lalala >test.lls

test.lls.s: test.lls
	llc test.lls

test: testHarness.c test.lls.s
	gcc testHarness.c test.lls.s -o test.exe
	test.exe | tee test.result

invaders: test.lls.s inv_src/invaders.c inv_src/invadersDebug.c inv_src/gui/font.h inv_src/gui/debugger.h inv_src/gui/debugger.c
	gcc -g -O3 inv_src/invaders.c inv_src/invadersDebug.c inv_src/gui/debugger.c test.lls.s $(GLLIBS) -o invaders.exe

