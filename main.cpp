#include "Berkeley.h"
#include "Epoll.h"

#include <iostream>

int main() {

    uS::Berkeley<uS::Epoll> c;

    enum SocketStates {
        HTTP_SOCKET
    };

    struct HttpState {
        static void onData(uS::Berkeley<uS::Epoll>::Socket *socket, char *data, size_t length) {
            std::cout << "HttpSocket got data: " << std::string(data, length) << std::endl;
        }

        static void onEnd(uS::Berkeley<uS::Epoll>::Socket *socket) {
            std::cout << "HttpSocket disconnected" << std::endl;

            socket->close();
        }
    };
    c.addSocketState<HttpState>(HTTP_SOCKET);

    // application decides how and what to allocate
    auto socketAllocator = [](uS::Berkeley<uS::Epoll> *context) {
        return new uS::Berkeley<uS::Epoll>::Socket(context);
    };

    // symmetric listen and connect
    if (c.listen(nullptr, 3000, 0, [](uS::Berkeley<uS::Epoll>::Socket *socket) {

        // getting nullptr for socket can be sign of error

        std::cout << "Client connected: " << sizeof(*socket) << std::endl;

        // all new sockets needs to have a state set!
        socket->setState(HTTP_SOCKET);

    }, socketAllocator)) {
        std::cout << "is now listening" << std::endl;
    }

    c.connect("localhost", 3000, [](uS::Berkeley<uS::Epoll>::Socket *socket) {
        if (!socket) {
            std::cout << "Connection failed" << std::endl;
        } else {
            std::cout << "We connected" << std::endl;

            socket->setState(HTTP_SOCKET);
        }
    }, socketAllocator);

    // context is the loop
    c.run();
}
