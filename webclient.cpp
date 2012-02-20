#include <iostream>
#include <memory>
#include <string>
#include "native.h"
using namespace native;

#include <native/net.h>

int main(int argc, char** argv)
{
    return run([=]() {
        net::createServer([](net::Server* server){
            server->on<ev::connection>([](net::Socket* socket){
                std::cout << "Accepted." << std::endl;
                socket->end("Hello, World!\r\n");
            });
            server->on<ev::error>([=](Exception e){
                std::cout << "Error: " << e.message() << std::endl;
                server->close();
            });
            server->listen(1337, "127.0.0.1");
        });
    });
}
