.PHONY: all test clean

all:
	echo "make all"
	mkdir -p bin/include
	mkdir -p bin/lib
	g++ -c src/simple_config.cpp -o bin/simple_config.o
	g++ -c src/simple_log.cpp -o bin/simple_log.o
	ar -rcs libsimplelog.a bin/*.o
	
	cp src/*.h bin/include/
	mv libsimplelog.a bin/lib/
	
test: src/simple_log.cpp test/simple_log_test.cpp
	g++ -I bin/include test/simple_log_test.cpp bin/lib/libsimplelog.a -o bin/simple_log_test
	mkdir -p log
	./bin/simple_log_test