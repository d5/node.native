#include <iostream>
#include <native/native.h>
using namespace native;

int main(int argc, char** argv) {
    if(argc < 3) {
        std::cout << "usage: " << argv[0] << " SRC_FILE DEST_FILE" << std::endl;
        return 1;
    }

    file::read(argv[1], [=](const std::string& str, error e){
        if(e) {
            std::cout << "file::read() failed: " << e.str() << std::endl;
        } else {
            std::cout << str.length() << " bytes read from: " << argv[1] << std::endl;
            file::write(argv[2], str, [=](int nwritten, error e){
                if(e) {
                    std::cout << "file::write() failed: " << e.str() << std::endl;
                } else {
                    std::cout << nwritten << " bytes written to: " << argv[2] << std::endl;
                }
            });
        }
    });

    return run();
}
