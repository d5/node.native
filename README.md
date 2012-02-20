# node.native 

<b>node.native</b> is a [C++11](http://en.wikipedia.org/wiki/C%2B%2B11) (aka C++0x) port for [node.js](https://github.com/joyent/node). 

Please note that <b>node.native</b> project is <em>under heavy development</em>: currently experimental.

## Sample code

Simple echo server:

    #include "native.h"
    using namespace native;
    
    int main(int argc, char** argv) {
        return run([=]() {
            net::createServer([](net::Server* server){
                server->on<ev::connection>([](net::Socket* socket){
                    socket->pipe(socket, {});
                });
                server->on<ev::error>([=](Exception e){
                    server->close();
                });
                server->listen(1337, "127.0.0.1");
            });
        });
    }

## Getting started

<em>node.native</em> consists of header files(*.h) only, but requires [libuv](https://github.com/joyent/libuv) and [http-parser](https://github.com/joyent/http-parser) lib to use.

To compile included sample application(webserver.cpp):

    export LIBUV_PATH=/path/to/libuv_dir
    export HTTP_PARSER_PATH=/path/to/http-parser_dir

then,

    make

I tested the code on Ubuntu 11.10 and GCC 4.6.1.

## Other Resources

- [Mailing list](http://groups.google.com/group/nodenative)
- [Public to-to lists](https://trello.com/b/1qk3tRGS)

You can also access this page via http://www.nodenative.com.