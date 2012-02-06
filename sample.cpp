#include <iostream>
#include <string>
#include "http.h"

using namespace std;
using namespace native::base;
using namespace native::http;

int main()
{
    http server([](request& req, response& res){
		res.set_status(200);
		res.set_header("Content-Type", "text/plain");
		res.end("Hello, World!\n", [=](int status) {});
	});
    server.listen("0.0.0.0", 8080);

	// TODO: integrate into http class
    return loop::run_default();
}
