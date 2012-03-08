#include "native.h"
using namespace native;

int main(int argc, char** argv) {
    return run([=]() {
        http::createServer([](http::Server* server){
            server->on<event::connection>([](net::Socket* socket){
                std::cout << "connection" << std::endl;
            });
            server->on<event::request>([](http::ServerRequest* req, http::ServerResponse* res){
                std::cout << "request" << std::endl;
                res->end(Buffer(std::string("path: ")+req->path()+"\r\n"));
            });
            server->on<event::error>([](const Exception& e){
                std::cout << "error: " << e.message() << std::endl;
            });
            server->listen(1337, "127.0.0.1");
        });
    });
}
