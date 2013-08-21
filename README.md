# node.native 

<b>node.native</b> is a [C++11](http://en.wikipedia.org/wiki/C%2B%2B11) (aka C++0x) port for [node.js](https://github.com/joyent/node). 

Please note that node.native project is <em>under heavy development</em>.

<b>I'm working on the first release at [v0.1.0](https://github.com/d5/node.native/tree/v0.1.0) branch.</b>

## Sample code

Simplest web-server example using node.native.
```cpp
#include <iostream>
#include <native/native.h>
using namespace native::http;

int main() {
    http server;
    if(!server.listen("0.0.0.0", 8080, [](request& req, response& res) {
        res.set_status(200);
        res.set_header("Content-Type", "text/plain");
        res.end("C++ FTW\n");
    })) return 1; // Failed to run server.

    std::cout << "Server running at http://0.0.0.0:8080/" << std::endl;
    return native::run();
}
```
## Getting started

<em>node.native</em> consists of header files(*.h) only, but requires [libuv](https://github.com/joyent/libuv) and [http-parser](https://github.com/joyent/http-parser) lib to use.

To compile included sample application(webserver.cpp) first run the following command in the project directory:
```bash
git submodule update --init
```
then,
```bash
make
```
alternatively you can set custom paths to http-parser and libuv if you dont want to use the submodules.

Tested on Ubuntu 11.10 and GCC 4.6.1. and OSX 10.8.2

## Other Resources

- [Mailing list](http://groups.google.com/group/nodenative)
- [Public to-to lists](https://trello.com/b/1qk3tRGS)
