CXXFLAGS = -std=gnu++0x -g -O0 -Ideps/libuv/include -Ideps/http-parser -I. -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64

OS_NAME=$(shell uname -s)
ifeq (${OS_NAME},Darwin)
	RTLIB=
else
	RTLIB=-lrt
endif

all: native.a echo

echo: native.a echo.cpp 
	$(CXX) $(CXXFLAGS) -o echo echo.cpp native.a $(RTLIB) -lm -lpthread

native.a: deps/libuv/uv.a deps/http-parser/http_parser.o native.o
	mkdir -p objs
	cd objs; \
	ar x ../deps/libuv/uv.a; \
	cp ../deps/http-parser/http_parser.o .; \
	cp ../native.o .; \
	ar rcs native.a *.o; \
	cp native.a ../; \
	cd ..; \
	rm -rf objs	

deps/libuv/uv.a:
	$(MAKE) -C deps/libuv

deps/http-parser/http_parser.o:
	$(MAKE) -C deps/http-parser http_parser.o
	
native.o: native.cpp native.h $(wildcard native/*.h) $(wildcard native/detail/*.h) 
	$(CXX) $(CXXFLAGS) -o native.o -c native.cpp $(RTLIB) -lm -lpthread	
	
clean:
	rm -f deps/libuv/uv.a
	rm -f deps/http-parser/http_parser.o
	rm -f native.a
	rm -f echo
	rm -rf objs
