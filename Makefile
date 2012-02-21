CXXFLAGS = -std=gnu++0x -Iext/libuv/include -Iext/http-parser -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64
CXXFLAGS_DEBUG = $(CXXFLAGS) -g -O0

OS_NAME=$(shell uname -s)
ifeq (${OS_NAME},Darwin)
	RTLIB=
else
	RTLIB=-lrt
endif

all: native.a echo

debug: echo_debug

echo: native.a echo.cpp 
	$(CXX) $(CXXFLAGS) -o echo echo.cpp native.a $(RTLIB) -lm -lpthread
	
echo_debug: echo.cpp native.h $(wildcard native/*.h) $(wildcard native/detail/*.h) ext/libuv/uv.a ext/http-parser/http_parser.o
	$(CXX) $(CXXFLAGS_DEBUG) -o echo_debug echo.cpp ext/libuv/uv.a ext/http-parser/http_parser.o $(RTLIB) -lm -lpthread	

native.a: ext/libuv/uv.a ext/http-parser/http_parser.o native.o
	mkdir -p objs
	cd objs; \
	ar x ../ext/libuv/uv.a; \
	cp ../ext/http-parser/http_parser.o .; \
	cp ../native.o .; \
	ar rcs native.a *.o; \
	cp native.a ../; \
	cd ..; \
	rm -rf objs	

ext/libuv/uv.a:
	$(MAKE) -C ext/libuv

ext/http-parser/http_parser.o:
	$(MAKE) -C ext/http-parser http_parser.o
	
native.o: native.cpp native.h $(wildcard native/*.h) $(wildcard native/detail/*.h) 
	$(CXX) $(CXXFLAGS) -o native.o -c native.cpp $(RTLIB) -lm -lpthread
	
	
clean:
	rm -f ext/libuv/uv.a
	rm -f ext/http-parser/http_parser.o
	rm -f native.a
	rm -f echo
	rm -f echo_debug
	rm -rf objs
