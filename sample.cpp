#include <iostream>
#include <string>
#include "base.h"
#include "http.h"

using namespace std;
using namespace native::base;
using namespace native::http;

int main()
{
    cout << "Starting test server..." << endl << endl;
    {
		tcp_ptr server(new tcp);
		server->bind("0.0.0.0", 8080);

		server->listen([=](int status)
		{
			tcp_ptr client(new tcp);

			server->accept(client);
			client->read_start([=](const char* buf, int len)
			{
				if(len > 0)
				{
					cout << string(buf, len) << endl;
				}

				client->read_stop();

				client->close([=](){
					server->close([=](){
						cout << "Server closed." << endl;
					});
				});
			});
		});
    }
    return loop::run_default();
}
