#include <iostream>
#include <memory>
#include <string>
#include "native.h"
using namespace native;

#include <native/net.h>

int main() {
    auto server = net::createServer([](std::shared_ptr<net::Socket> socket){
        std::cout << "Accepted" << std::endl;
    });
    server->listen(1337, "127.0.0.1");

    return Node::instance().start();
}
