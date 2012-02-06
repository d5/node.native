#include <iostream>
#include <string>
#include "base.h"
#include "http.h"

using namespace std;
using namespace native::base;
using namespace native::http;

int main()
{
    http server([](request& req, response& res){
		string response_text("HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nHello, World!\n");
		res.write(response_text, [=](int status) {
			// ...
		});
	});
    server.listen("0.0.0.0", 8080);

	// TODO: integrate into http class
    return loop::run_default();
}
