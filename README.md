# node.native 

<em>node.native</em> is a [C++11](http://en.wikipedia.org/wiki/C%2B%2B11) port for [node.js](https://github.com/joyent/http-parser). 

<b>node.native</b> is under heady development stage.

## Sample code

Simplest web-server example using node.native.

    #include "http.h"
    using namespace native::http;
    
    int main()
    {
        http server([](request& req, response& res){
            res.set_status(200);
            res.set_header("Content-Type", "text/plain");
            res.end("Hello, World!\n", [=](int status) {});
        });
        server.listen("0.0.0.0", 8080);

        return loop::run_default();
    }
