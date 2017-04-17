#include "Berkeley.h"
#include "Epoll.h"

#include <iostream>

int main() {

    uS::Berkeley<uS::Epoll> c;

    enum States {
        HTTP_SOCKET
    };

    struct HttpSocket : uS::Berkeley<uS::Epoll>::Socket {

        HttpSocket(uS::Berkeley<uS::Epoll> *context) : uS::Berkeley<uS::Epoll>::Socket(context) {
            setState(HTTP_SOCKET);
        }

        static void onData(uS::Berkeley<uS::Epoll>::Socket *socket, char *data, size_t length) {
            std::cout << "HttpSocket got data: " << std::string(data, length) << std::endl;
        }

        static void onEnd(uS::Berkeley<uS::Epoll>::Socket *socket) {
            HttpSocket *httpSocket = (HttpSocket *) socket;

            httpSocket->close();
            delete httpSocket;
        }

        static uS::Berkeley<uS::Epoll>::Socket *allocator(uS::Berkeley<uS::Epoll> *context) {
            return new HttpSocket(context);
        }
    };

    c.addSocketState<HttpSocket>(HTTP_SOCKET);

    if (c.listen(nullptr, 3000, 0, [](uS::Berkeley<uS::Epoll>::Socket *socket) {
        std::cout << "Someone connected" << std::endl;
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
