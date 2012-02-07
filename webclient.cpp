#include <iostream>
#include <memory>
#include <string>
#include <native/native.h>
using namespace native;

#include <fcntl.h>

int main() {
    fs::open("Makefile", O_RDONLY, 0, [](file f){
#if 0
        char buf[10000];
        fs::read(f, buf, 10000, 0, [](uv_fs_t* req){
            printf("test\n");
        });
#endif
    });

    return run();

    auto client = net::tcp::create();
    client->connect("127.0.0.1", 8080, [=](error e){
        client->write("GET / HTTP/1.1\r\n\r\n", [=](error e){
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

    return run();
}
