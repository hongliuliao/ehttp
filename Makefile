.PHONY: all test clean deps test-deps tags 

CXX=g++
CXXFLAGS += -g -Wall
LDFLAGS += -pthread

MULTIPART_PARSER_INC_PATH=-I deps/multipart-parser-c/
MULTIPART_PARSER_LIB_PATH=deps/multipart-parser-c/multipart_parser.o

DEPS_INCLUDE_PATH=-I deps/json-cpp/include/ -I deps/http-parser/ $(MULTIPART_PARSER_INC_PATH)
DEPS_LIB_PATH=deps/json-cpp/output/lib/libjson_libmt.a deps/http-parser/libhttp_parser.a $(MULTIPART_PARSER_LIB_PATH)

GTEST_INC=-I deps/googletest/googletest/include
GTEST_LIB=deps/googletest/googletest/make/gtest_main.a

SRC_INCLUDE_PATH=-I src
OUTPUT_INCLUDE_PATH=-I output/include
OUTPUT_LIB_PATH=output/lib/libsimpleserver.a

objects := $(patsubst %.cpp,%.o,$(wildcard src/*.cpp))

all: libsimpleserver.a

prepare: 
	mkdir -p output/include output/lib output/bin
	cp src/*.h output/include/

tags:
	ctags -R /usr/include src deps

deps: prepare 
	make -C deps/http-parser package
	make -C deps/json-cpp
	make -C deps/multipart-parser-c

test-deps:
	make -C deps/googletest/googletest/make

libsimpleserver.a: deps $(objects)
	ar -rcs libsimpleserver.a src/*.o
	mv libsimpleserver.a output/lib/

test: libsimpleserver.a http_server_test http_parser_test issue5_server

%.o: %.cpp
	$(CXX) -c $(CXXFLAGS) $(DEPS_INCLUDE_PATH) $(SRC_INCLUDE_PATH) $< -o $@

http_server_test: test/http_server_test.cpp
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(DEPS_INCLUDE_PATH) $(OUTPUT_INCLUDE_PATH) $< $(OUTPUT_LIB_PATH) $(DEPS_LIB_PATH) -o output/bin/$@

threadpool_test: test/threadpool_test.cpp test-deps libsimpleserver.a
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(DEPS_INCLUDE_PATH) $(OUTPUT_INCLUDE_PATH) $(GTEST_INC) $< $(OUTPUT_LIB_PATH) $(DEPS_LIB_PATH) $(GTEST_LIB) -o output/bin/$@

http_multi_thread_demo: test/http_multi_thread_demo.cpp test-deps libsimpleserver.a
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(DEPS_INCLUDE_PATH) $(OUTPUT_INCLUDE_PATH) $(GTEST_INC) $< $(OUTPUT_LIB_PATH) $(DEPS_LIB_PATH) $(GTEST_LIB) -o output/bin/$@

http_multipart_demo: test/http_multipart_demo.cpp libsimpleserver.a
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(DEPS_INCLUDE_PATH) $(OUTPUT_INCLUDE_PATH) $(GTEST_INC) $< $(OUTPUT_LIB_PATH) $(DEPS_LIB_PATH) -o output/bin/$@

http_parser_test: test/http_parser_test.cpp test-deps
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(DEPS_INCLUDE_PATH) $(OUTPUT_INCLUDE_PATH) $(GTEST_INC) $< $(OUTPUT_LIB_PATH) $(DEPS_LIB_PATH) $(GTEST_LIB) -o output/bin/$@


issue5_server: test/issue5/issue5_server.cpp
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(DEPS_INCLUDE_PATH) $(OUTPUT_INCLUDE_PATH) $< $(OUTPUT_LIB_PATH) $(DEPS_LIB_PATH) -o output/bin/$@

clean:
	rm -rf src/*.o
	rm -rf output/*
	make -C deps/multipart-parser-c clean
	make -C deps/googletest/googletest/make clean
