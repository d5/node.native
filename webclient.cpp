#include <iostream>
#include <memory>
#include <string>
#include "native.h"
using namespace native;

#include <native/net.h>

int main() {
    auto server = net::createServer([](net::Socket& socket){
        // ...
    });
    //server.listen(1337, "127.0.0.1");

    return Node::instance().start();
}
