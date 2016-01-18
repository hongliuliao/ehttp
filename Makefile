.PHONY: all test clean deps tags

CXX=g++
CXXFLAGS += -g -Wall
LDFLAGS += -lpthread

DEPS_INCLUDE_PATH=-I deps/simple_log/output/include/ -I deps/json-cpp/include/ -I deps/http-parser/
DEPS_LIB_PATH=deps/simple_log/output/lib/libsimplelog.a deps/json-cpp/output/lib/libjson_libmt.a deps/http-parser/libhttp_parser.a
SRC_INCLUDE_PATH=-I src
OUTPUT_INCLUDE_PATH=-I output/include
OUTPUT_LIB_PATH=output/lib/libsimpleserver.a

objects := $(patsubst %.cpp,%.o,$(wildcard src/*.cpp))


all: libsimpleserver.a
	mkdir -p output/include output/lib output/bin
	cp src/*.h output/include/
	mv libsimpleserver.a output/lib/
	rm -rf src/*.o

tags:
	ctags -R /usr/include src deps

deps:
	make -C deps/http-parser package
	make -C deps/simple_log
	make -C deps/json-cpp

libsimpleserver.a: deps $(objects) 
	ar -rcs libsimpleserver.a src/*.o

test: http_server_test http_parser_test
	
%.o: %.cpp
	$(CXX) -c $(CXXFLAGS) $(DEPS_INCLUDE_PATH) $(SRC_INCLUDE_PATH) $< -o $@

http_server_test: test/http_server_test.cpp
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(DEPS_INCLUDE_PATH) $(OUTPUT_INCLUDE_PATH) $< $(OUTPUT_LIB_PATH) $(DEPS_LIB_PATH) -o output/bin/$@
	
http_parser_test: test/http_parser_test.cpp
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(DEPS_INCLUDE_PATH) $(OUTPUT_INCLUDE_PATH) $< $(OUTPUT_LIB_PATH) $(DEPS_LIB_PATH) -o output/bin/$@

clean:
	rm -rf output/*

