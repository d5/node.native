#include <iostream>
#include <native/native.h>
using namespace native;

// usage: (executable)  src_file  dest_file

int main(int argc, char** argv) {
    if(argc < 3) {
        std::cout << "usage: " << argv[0] << " SRC_FILE DEST_FILE" << std::endl;
        return 1;
    }

    // read contents from source and then write it to destination.
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
