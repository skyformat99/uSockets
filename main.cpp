#include "Context.h"
#include "Berkeley.h"
#include "Epoll.h"

#include <iostream>

int main() {

    uS::Context<uS::Berkeley<uS::Epoll>> c;

    // application decides how and what to allocate
    auto socketAllocator = [](uS::Context<uS::Berkeley<uS::Epoll>> *context) -> uS::Socket<uS::Berkeley<uS::Epoll>> * {
        return new uS::Socket<uS::Berkeley<uS::Epoll>>(context);
    };

    // symmetric listen and connect
    c.listen(nullptr, 3000, [](uS::Socket<uS::Berkeley<uS::Epoll>> *socket) {

        // getting nullptr for socket can be sign of error

        std::cout << "Client connected!" << std::endl;
    }, socketAllocator);

    c.connect("localhost", 3000, [](uS::Socket<uS::Berkeley<uS::Epoll>> *socket) {
        if (!socket) {
            std::cout << "Connection failed" << std::endl;
        } else {
            std::cout << "We connected" << std::endl;
        }
    }, socketAllocator);

    // context is the loop
    c.run();
}
