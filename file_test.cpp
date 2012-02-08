#include <iostream>
#include <native/native.h>
using namespace native;

int main(int argc, char** argv) {
    if(argc < 2) {
        std::cout << "usage: " << argv[0] << " FILE" << std::endl;
        return 1;
    }

    file::read(argv[1], [](const std::string& str, error e){
        if(e) std::cout << "Error: " << e.str() << std::endl;
        else std::cout << str << std::endl;
    });

    return run();
}
