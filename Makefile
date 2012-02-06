PROGNAME = sample
LIBS = -lrt -lm -lpthread
LIBUV_INC = $(LIBUV_PATH)/include
HTTP_PARSER_INC = $(HTTP_PARSER_PATH)
INCLUDES = -I$(LIBUV_INC) -I$(HTTP_PARSER_INC)
LDFLAGS = 
OBJECTS = sample.o $(LIBUV_PATH)/uv.a $(HTTP_PARSER_PATH)/http_parser.o
CPPFLAGS = -std=gnu++0x
CPPFLAGS_DEBUG = $(CPPFLAGS) -g -O0

all: env_req $(PROGNAME)

$(PROGNAME): $(OBJECTS)
	g++ -o $(PROGNAME) $(OBJECTS) $(LIBS) $(INCLUDES) $(LDFLAGS)

$(OBJECTS): Makefile

$(LIBUV_PATH)/uv.a:
	$(MAKE) -C $(LIBUV_PATH)

$(HTTP_PARSER_PATH)/http_parser.o:
	$(MAKE) -C $(HTTP_PARSER_PATH) http_parser.o

sample.o: sample.cpp $(wildcard *.h)
	g++ -c $(CPPFLAGS_DEBUG) $(INCLUDES) -o sample.o sample.cpp 
    
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
	rm *.o $(PROGNAME)