#include <iostream>
#include "native.h"
using namespace native;

int main() {
    Node::instance().init();

    /*
    detail::http server;
    if(!server.listen("0.0.0.0", 8080, [](detail::request& req, detail::response& res) {
        res.set_status(200);
        res.set_header("Content-Type", "text/plain");
        res.end("C++ FTW\n");
    })) return 1; // Failed to run server.

    std::cout << "Server running at http://0.0.0.0:8080/" << std::endl;
     */

    return Node::instance().start();
}
