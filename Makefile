.PHONY: all test clean deps test-deps tags 

CXX=g++
CXXFLAGS += -g -Wall
LDFLAGS += -pthread

ifdef ACOV
	CXXFLAGS += -fprofile-arcs -ftest-coverage -fsanitize=address
	LDFLAGS += -lgcov --coverage -fsanitize=address
endif

MULTIPART_PARSER_INC_PATH=-I deps/multipart-parser-c/
MULTIPART_PARSER_LIB_PATH=deps/multipart-parser-c/multipart_parser.o

DEPS_INCLUDE_PATH=-I deps/json-cpp/include/ -I deps/http-parser/ $(MULTIPART_PARSER_INC_PATH)
DEPS_LIB_PATH=deps/json-cpp/output/lib/libjson_libmt.a deps/http-parser/libhttp_parser.a $(MULTIPART_PARSER_LIB_PATH)

GTEST_INC=-I deps/googletest/googletest/include
GTEST_LIB=deps/googletest/googletest/make/gtest_main.a

SRC_INCLUDE_PATH=-I src
OUTPUT_INCLUDE_PATH=-I output/include
OUTPUT_LIB_PATH=output/lib/libehttp.a
OUTPUT_TEST_DIR=output/test

objects := $(patsubst %.cpp,%.o,$(wildcard src/*.cpp))

all: libehttp.a

prepare: 
	mkdir -p output/include output/lib output/bin output/test
	cp src/*.h output/include/

tags:
	ctags -R /usr/include src deps

deps: prepare 
	make -C deps/http-parser package
	make -C deps/json-cpp
	make -C deps/multipart-parser-c

test-deps:
	make -C deps/googletest/googletest/make

libehttp.a: deps $(objects)
	ar -rcs libehttp.a src/*.o
	mv libehttp.a output/lib/
	cp output/lib/libehttp.a output/lib/libsimpleserver.a

test: libehttp.a hello_server http_server_test sim_parser_test issue5_server threadpool_test string_utils_test simple_config_test simple_log_test epoll_socket_test

%.o: %.cpp
	$(CXX) -c $(CXXFLAGS) $(DEPS_INCLUDE_PATH) $(SRC_INCLUDE_PATH) $< -o $@

hello_server: test/hello_server.cpp
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(DEPS_INCLUDE_PATH) $(OUTPUT_INCLUDE_PATH) $< $(OUTPUT_LIB_PATH) $(DEPS_LIB_PATH) -o $(OUTPUT_TEST_DIR)/$@

http_server_test: test/http_server_test.cpp test-deps libehttp.a
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(DEPS_INCLUDE_PATH) $(OUTPUT_INCLUDE_PATH) $(GTEST_INC) $< $(OUTPUT_LIB_PATH) $(DEPS_LIB_PATH) $(GTEST_LIB) -o $(OUTPUT_TEST_DIR)/$@

threadpool_test: test/threadpool_test.cpp test-deps libehttp.a
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(DEPS_INCLUDE_PATH) $(OUTPUT_INCLUDE_PATH) $(GTEST_INC) $< $(OUTPUT_LIB_PATH) $(DEPS_LIB_PATH) $(GTEST_LIB) -o $(OUTPUT_TEST_DIR)/$@

http_multi_thread_demo: test/http_multi_thread_demo.cpp test-deps libehttp.a
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(DEPS_INCLUDE_PATH) $(OUTPUT_INCLUDE_PATH) $(GTEST_INC) $< $(OUTPUT_LIB_PATH) $(DEPS_LIB_PATH) $(GTEST_LIB) -o $(OUTPUT_TEST_DIR)/$@

http_multipart_demo: test/http_multipart_demo.cpp libehttp.a
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(DEPS_INCLUDE_PATH) $(OUTPUT_INCLUDE_PATH) $(GTEST_INC) $< $(OUTPUT_LIB_PATH) $(DEPS_LIB_PATH) -o $(OUTPUT_TEST_DIR)/$@

sim_parser_test: test/sim_parser_test.cpp test-deps
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(DEPS_INCLUDE_PATH) $(OUTPUT_INCLUDE_PATH) $(GTEST_INC) $< $(OUTPUT_LIB_PATH) $(DEPS_LIB_PATH) $(GTEST_LIB) -o $(OUTPUT_TEST_DIR)/$@

string_utils_test: test/string_utils_test.cpp test-deps
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(DEPS_INCLUDE_PATH) $(OUTPUT_INCLUDE_PATH) $(GTEST_INC) $< $(OUTPUT_LIB_PATH) $(DEPS_LIB_PATH) $(GTEST_LIB) -o $(OUTPUT_TEST_DIR)/$@

simple_config_test: test/simple_config_test.cpp test-deps
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(DEPS_INCLUDE_PATH) $(OUTPUT_INCLUDE_PATH) $(GTEST_INC) $< $(OUTPUT_LIB_PATH) $(DEPS_LIB_PATH) $(GTEST_LIB) -o $(OUTPUT_TEST_DIR)/$@

simple_log_test: test/simple_log_test.cpp test-deps
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(DEPS_INCLUDE_PATH) $(OUTPUT_INCLUDE_PATH) $(GTEST_INC) $< $(OUTPUT_LIB_PATH) $(DEPS_LIB_PATH) $(GTEST_LIB) -o $(OUTPUT_TEST_DIR)/$@

epoll_socket_test: test/epoll_socket_test.cpp test-deps
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(DEPS_INCLUDE_PATH) $(OUTPUT_INCLUDE_PATH) $(GTEST_INC) $< test/mock_sys_func.cpp $(OUTPUT_LIB_PATH) $(DEPS_LIB_PATH) $(GTEST_LIB) -o $(OUTPUT_TEST_DIR)/$@

issue5_server: test/issue5/issue5_server.cpp
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(DEPS_INCLUDE_PATH) $(OUTPUT_INCLUDE_PATH) $< $(OUTPUT_LIB_PATH) $(DEPS_LIB_PATH) -o $(OUTPUT_TEST_DIR)/$@

clean:
	rm -rf *.gcda
	rm -rf *.gcno
	rm -rf src/*.o
	rm -rf src/*.gcda
	rm -rf src/*.gcno
	rm -rf output/*
	make -C deps/multipart-parser-c clean
	make -C deps/googletest/googletest/make clean
	make -C deps/http-parser clean
	make -C deps/json-cpp clean
