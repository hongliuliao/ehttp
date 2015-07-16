.PHONY: all test clean

CXX=g++
CXXFLAGS += -g

DEPS_INCLUDE_PATH=-I dependency/simple_log/include/ -I dependency/json-cpp/include/
SRC_INCLUDE_PATH=-I src
OUTPUT_INCLUDE_PATH=-I bin/include
OUTPUT_LIB_PATH=bin/lib/libsimpleserver.a
DEPS_LIB_PATH=dependency/simple_log/lib/libsimplelog.a dependency/json-cpp/lib/libjson_libmt.a

objects := $(patsubst %.cpp,%.o,$(wildcard src/*.cpp))

all: libsimpleserver.a
	mkdir -p bin/include bin/lib
	cp src/*.h bin/include/
	mv libsimpleserver.a bin/lib/
	rm -rf src/*.o

libsimpleserver.a: $(objects) 
	ar -rcs libsimpleserver.a src/*.o

test: http_server_test http_parser_test
	
%.o: %.cpp
	$(CXX) -c $(CXXFLAGS) $(DEPS_INCLUDE_PATH) $(SRC_INCLUDE_PATH) $< -o $@

http_server_test: test/http_server_test.cpp
	$(CXX) $(CXXFLAGS) $(DEPS_INCLUDE_PATH) $(OUTPUT_INCLUDE_PATH) $< $(OUTPUT_LIB_PATH) $(DEPS_LIB_PATH) -o bin/$@
	
http_parser_test: test/http_parser_test.cpp
	$(CXX) $(CXXFLAGS) $(DEPS_INCLUDE_PATH) $(OUTPUT_INCLUDE_PATH) $< $(OUTPUT_LIB_PATH) $(DEPS_LIB_PATH) -o bin/$@

clean:
	rm -rf bin/*

