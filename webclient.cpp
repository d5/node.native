#include <iostream>
#include <memory>
#include <string>
#include <native/native.h>
using namespace native::net;

int main() {
    auto client = tcp::create();
    client->connect("127.0.0.1", 8080, [=](int status){
        client->write("GET / HTTP/1.1\r\n\r\n", [=](int status){
            std::shared_ptr<std::string> response(new std::string);
            client->read_start([=](const char* buf, ssize_t len){
                if(len < 0) // EOF
                {
                    std::cout << *response << std::endl;
                    client->close([](){});
                }
                else
                {
                    response->append(std::string(buf, len));
                }
            });
        });
    });

    return native::run();
}
