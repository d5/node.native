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
		cout << "Hi Client!" << endl;
		cout << "PATH: " << req.url().path() << endl;
	});
    printf("server: %x\n", &server);
    server.listen("0.0.0.0", 8080);

	// TODO: integrate into http class
    return loop::run_default();
}
