.PHONY: all test clean

INC_PATH=-I include

all:
	echo "make all"
	mkdir -p output/include
	mkdir -p output/lib
	g++ -c $(INC_PATH) src/lib_json/json_reader.cpp -o src/lib_json/json_reader.o
	g++ -c $(INC_PATH) src/lib_json/json_writer.cpp -o src/lib_json/json_writer.o
	g++ -c $(INC_PATH) src/lib_json/json_value.cpp -o src/lib_json/json_value.o
	ar -rcs libjson_libmt.a src/lib_json/*.o
	rm -rf src/lib_json/*.o
	
	cp -r include/json output/include/
	mv libjson_libmt.a output/lib/
	
