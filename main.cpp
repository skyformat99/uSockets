#include "Berkeley.h"
#include "Epoll.h"

#include <iostream>

int main() {

    uS::Berkeley<uS::Epoll> c;

    // application decides how and what to allocate
    auto socketAllocator = [](uS::Berkeley<uS::Epoll> *context) -> uS::Berkeley<uS::Epoll>::Socket * {
        return new uS::Berkeley<uS::Epoll>::Socket(context);
    };

    // symmetric listen and connect
    if (c.listen(nullptr, 3000, 0, [](uS::Berkeley<uS::Epoll>::Socket *socket) {

        // getting nullptr for socket can be sign of error

        std::cout << "Client connected!" << std::endl;

        socket->close();

    }, socketAllocator)) {
        std::cout << "is now listening" << std::endl;
    }

    c.connect("localhost", 3000, [](uS::Berkeley<uS::Epoll>::Socket *socket) {
        if (!socket) {
            std::cout << "Connection failed" << std::endl;
        } else {
            std::cout << "We connected" << std::endl;
        }
    }, socketAllocator);

    // context is the loop
    c.run();
}
