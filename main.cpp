#include "Berkeley.h"
#include "Epoll.h"

#include <iostream>

enum States {
    HTTP_SOCKET
};

struct HttpSocket : uS::Berkeley<uS::Epoll>::Socket {
    HttpSocket(uS::Berkeley<uS::Epoll> *context) : uS::Berkeley<uS::Epoll>::Socket(context) {
        setDerivative(HTTP_SOCKET);
    }

    static void onData(uS::Berkeley<uS::Epoll>::Socket *socket, char *data, size_t length) {
        std::cout << "HttpSocket got data: " << std::string(data, length) << std::endl;
    }

    static void onEnd(uS::Berkeley<uS::Epoll>::Socket *socket) {
        socket->close([](uS::Berkeley<uS::Epoll>::Socket *socket) {
            delete (HttpSocket *) socket;
        });
    }

    static uS::Berkeley<uS::Epoll>::Socket *allocator(uS::Berkeley<uS::Epoll> *context) {
        return new HttpSocket(context);
    }
};

int main() {
    uS::Berkeley<uS::Epoll> c;
    c.registerSocketDerivative<HttpSocket>(HTTP_SOCKET);

    if (c.listen(nullptr, 3000, 0, [](uS::Berkeley<uS::Epoll>::Socket *socket) {
        socket->send("Hello there, client!", 20);
    }, HttpSocket::allocator)) {
        std::cout << "Listening to port 3000" << std::endl;
    }

    c.connect("localhost", 3000, [](uS::Berkeley<uS::Epoll>::Socket *socket) {
        if (!socket) {
            std::cout << "Connection failed" << std::endl;
        } else {
            std::cout << "We connected" << std::endl;
        }
    }, HttpSocket::allocator);

    c.run();
}
