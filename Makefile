HTTP_PARSER_PATH = http-parser
LIBUV_PATH = libuv

LIBUV_NAME=libuv.a
OS_NAME=$(shell uname -s)
ifeq (${OS_NAME},Darwin)
	RTLIB=
	CXXFLAGS = -framework CoreFoundation -framework CoreServices -std=gnu++0x -stdlib=libc++ -g -O0 -I$(LIBUV_PATH)/include -I$(HTTP_PARSER_PATH) -I. -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64

else
	RTLIB=-lrt
	CXXFLAGS = -std=gnu++0x -g -O0 -I$(LIBUV_PATH)/include -I$(HTTP_PARSER_PATH) -I. -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64
endif

all: webclient webserver file_test

webclient: webclient.cpp $(LIBUV_PATH)/$(LIBUV_NAME) $(HTTP_PARSER_PATH)/http_parser.o $(wildcard native/*.h)
	$(CXX) $(CXXFLAGS) -o webclient webclient.cpp $(LIBUV_PATH)/$(LIBUV_NAME) $(HTTP_PARSER_PATH)/http_parser.o $(RTLIB) -lm -lpthread

webserver: webserver.cpp $(LIBUV_PATH)/$(LIBUV_NAME) $(HTTP_PARSER_PATH)/http_parser.o $(wildcard native/*.h)
	$(CXX) $(CXXFLAGS) -o webserver webserver.cpp $(LIBUV_PATH)/$(LIBUV_NAME) $(HTTP_PARSER_PATH)/http_parser.o $(RTLIB) -lm -lpthread

file_test: file_test.cpp $(LIBUV_PATH)/$(LIBUV_NAME) $(HTTP_PARSER_PATH)/http_parser.o $(wildcard native/*.h)
	$(CXX) $(CXXFLAGS) -o file_test file_test.cpp $(LIBUV_PATH)/$(LIBUV_NAME) $(HTTP_PARSER_PATH)/http_parser.o $(RTLIB) -lm -lpthread

$(LIBUV_PATH)/$(LIBUV_NAME):
	$(MAKE) -C $(LIBUV_PATH)

$(HTTP_PARSER_PATH)/http_parser.o:
	$(MAKE) -C $(HTTP_PARSER_PATH) http_parser.o

clean:
	$(MAKE) -C libuv clean
	$(MAKE) -C http-parser clean
	rm -f $(LIBUV_PATH)/$(LIBUV_NAME)
	rm -f $(HTTP_PARSER_PATH)/http_parser.o
	rm -f webclient webserver file_test


