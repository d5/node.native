#include <iostream>
#include <native/native.h>
using namespace native::http;

int main() {
    http server;
    int port = 8080;
    if(!server.listen("0.0.0.0", port, [](request& req, response& res) {
        std::string body = req.get_body(); // Now you can write a custom handler for the body content.
        res.set_status(200);
        res.set_header("Content-Type", "text/plain");
        res.end("C++ FTW\n");
    })) return 1; // Failed to run server.

    std::cout << "Server running at http://0.0.0.0:" << port << "/" << std::endl;
    return native::run();
}
