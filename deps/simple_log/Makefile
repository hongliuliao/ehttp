.PHONY: all test clean

all:
	echo "make all"
	mkdir -p output/include
	mkdir -p output/lib
	mkdir -p output/bin
	g++ -c src/simple_config.cpp -o src/simple_config.o
	g++ -c src/simple_log.cpp -o src/simple_log.o
	ar -rcs libsimplelog.a src/*.o
	rm -rf src/*.o
	
	cp src/*.h output/include/
	mv libsimplelog.a output/lib/
	
test: src/simple_log.cpp test/simple_log_test.cpp
	g++ -I output/include test/simple_log_test.cpp output/lib/libsimplelog.a -o output/bin/simple_log_test
	mkdir -p log
