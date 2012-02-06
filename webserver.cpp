#include <iostream>
#include <native/http.h>
using namespace native::http;

int main()
{
	http server;
    if(server.listen("0.0.0.0", 8080, [](request& req, response& res){
		res.set_status(200);
		res.set_header("Content-Type", "text/plain");
		res.end("C++ FTW\n");
	})) std::cout << "Server running at http://0.0.0.0:8080/" << std::endl;

    return native::run();
}
