all: bin/edl

COMPILER=g++

OBJS = out/parser.o  \
       out/generator.o \
       out/main.o    \
       out/lexer.o  \

LLVM_MODULES = all

#TODO Fix so i can build in linux again
LLVM_CONFIG=llvm-config
START_GROUP=#--start-group
END_GROUP=#--end-group

CPPFLAGS = -std=c++11 -g -O0 -Isrc -Iout `$(LLVM_CONFIG) --cppflags`
LDFLAGS = -g `$(LLVM_CONFIG) --ldflags`
LIBS = `$(LLVM_CONFIG) --libs $(LLVM_MODULES)` `$(LLVM_CONFIG) --system-libs` -lpthread -lpsapi -limagehlp -lz

clean:
	$(RM) -rf out/*
	rmdir out
	$(RM) -rf bin/*
	rmdir bin

out/parser.cpp: src/edl.y 
	mkdir -p out
	bison -d -o $@ $^
	
out/parser.hpp: out/parser.cpp

out/lexer.cpp: src/edl.l out/parser.hpp
	flex -o $@ $^

out/parser.o: out/parser.cpp
	$(COMPILER) -c $(CPPFLAGS) -o $@ $<

out/lexer.o: out/lexer.cpp
	$(COMPILER) -c $(CPPFLAGS) -o $@ $<

out/%.o: src/%.cpp
	$(COMPILER) -c $(CPPFLAGS) -o $@ $<


bin/edl: $(OBJS)
	mkdir -p bin
	$(COMPILER) -o $@ $(LDFLAGS) $(START_GROUP) $(OBJS) $(LIBS) $(END_GROUP)
