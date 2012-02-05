LIBUV_INC = $(LIBUV_PATH)/include
HTTP_PARSER_INC = $(HTTP_PARSER_PATH)
CPPFLAGS = -std=gnu++0x -I$(LIBUV_INC) -I$(HTTP_PARSER_INC)
CPPFLAGS_DEBUG = $(CPPFLAGS) -g -O0

all: test

test: env_req test.cpp uv11.h libuv/uv.a http-parser/http_parser.o
	g++ $(CPPFLAGS_DEBUG) -o test test.cpp $(LIBUV_PATH)/uv.a $(HTTP_PARSER_PATH)/http_parser.o -lrt -lm -lpthread

libuv/uv.a:
	$(MAKE) -C $(LIBUV_PATH)

http-parser/http_parser.o:
	$(MAKE) -C $(HTTP_PARSER_PATH) http_parser.o
	
env_req:
ifndef LIBUV_PATH
	$(error Variable 'LIBUV_PATH' must be set to the path of libuv library.)
endif
ifndef HTTP_PARSER_PATH
	$(error Variable 'HTTP_PARSER_PATH' must be set to the path of http-parser library.)
endif

clean:
	rm -f $(LIBUV_PATH)/uv.a
	rm -f $(HTTP_PARSER_PATH)/http_parser.o
	rm -f *.o
	rm -f *.a
	rm -f test
