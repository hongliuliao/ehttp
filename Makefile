all:
	mkdir -p bin/include
	mkdir -p bin/lib
	cp src/*.h bin/include/
	g++ -g -c -I dependency/simple_log/include/ -I dependency/json-cpp/include/ -I src src/http_parser.cpp -o bin/http_parser.o
	g++ -g -c -I dependency/simple_log/include/ -I dependency/json-cpp/include/ -I src src/http_server.cpp -o bin/http_server.o
	g++ -g -c -I dependency/simple_log/include/ -I dependency/json-cpp/include/ -I src src/epoll_socket.cpp -o bin/epoll_socket.o
	ar -rcs libsimpleserver.a bin/*.o
	mv libsimpleserver.a bin/lib/
	rm -rf bin/*.o
test: test/http_server_test.cpp test/http_parser_test.cpp
	g++ -g -I dependency/simple_log/include/ -I dependency/json-cpp/include/ -I bin/include test/http_server_test.cpp dependency/simple_log/lib/libsimplelog.a dependency/json-cpp/lib/libjson_libmt.a bin/lib/libsimpleserver.a -lcurl -o bin/http_server_test
	g++ -I dependency/simple_log/include/ -I dependency/json-cpp/include/ -I bin/include test/http_parser_test.cpp dependency/simple_log/lib/libsimplelog.a bin/lib/libsimpleserver.a dependency/json-cpp/lib/libjson_libmt.a -lcurl -o bin/http_parser_test
	
clean:
	rm -rf bin/*