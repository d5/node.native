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
