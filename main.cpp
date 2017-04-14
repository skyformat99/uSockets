#include "Context.h"

#include <iostream>

int main() {

    uS::Context c;

    // application decides how and what to allocate
    auto socketAllocator = [](uS::Context *context) -> uS::Socket * {
        return new uS::Socket(context);
    };

    // symmetric listen and connect
    c.listen(nullptr, 3000, [](uS::Socket *socket) {

        // getting nullptr for socket can be sign of error

        std::cout << "Client connected!" << std::endl;
    }, socketAllocator);

    c.connect("localhost", 3000, [](uS::Socket *socket) {
        if (!socket) {
            std::cout << "Connection failed" << std::endl;
        } else {
            std::cout << "We connected" << std::endl;
        }
    }, socketAllocator);

    // context is the loop
    c.run();
}
