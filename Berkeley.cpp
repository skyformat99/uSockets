#include "Berkeley.h"
#include "Epoll.h"

#include <iostream>

namespace uS {

template <class Impl>
Berkeley<Impl>::Berkeley() : Impl(true) {

}

template <class Impl>
void Berkeley<Impl>::listen(const char *host, int port, std::function<void(Socket *socket)> acceptHandler, std::function<Socket *(Berkeley *)> socketAllocator) {

    if (!socketAllocator) {
        socketAllocator = defaultSocketAllocator;
    }

    listenData.push_back({host, port, acceptHandler, socketAllocator});
}

template <class Impl>
void Berkeley<Impl>::connect(const char *host, int port, std::function<void(Socket *socket)> acceptHandler, std::function<Socket *(Berkeley *)> socketAllocator) {

    if (!socketAllocator) {
        socketAllocator = defaultSocketAllocator;
    }

    // for now
    listenData.push_back({host, port, acceptHandler, socketAllocator});
}

template <class Impl>
void Berkeley<Impl>::Socket::close() {
    std::cout << "Closing socket" << std::endl;
}

template class Berkeley<Epoll>;

}
