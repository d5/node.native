#include <iostream>
#include "http.h"
using namespace native::http;

int main()
{
    http server([](request& req, response& res){
		res.set_status(200);
		res.set_header("Content-Type", "text/plain");
		res.end("C++ FTW!\n", [=](int status) {});
	});
    std::cout << "Server running at http://0.0.0.0:8080/" << std::endl;
    server.listen("0.0.0.0", 8080);

    return 0;
}
