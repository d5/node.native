#include <iostream>
#include <memory>
#include <string>
#include "native.h"
using namespace native;

#include <native/net.h>

int main() {
    Node::instance().init();

    auto server = net::createServer([](net::SocketPtr socket){
        std::cout << "Accepted" << std::endl;
    });
    server->on<ev::error>([](Exception e){
        std::cout << "Error: " << e.message() << std::endl;
    });
    server->listen(1337, "127.0.0.1");

    return Node::instance().start();
}
