#include "Context.h"
#include "Berkeley.h"
#include "Epoll.h"

namespace uS {

template <class Impl>
Context<Impl>::Context() {
    defaultSocketAllocator = [](Context *context) {
        return new Socket<Impl>(context);
    };
}

template <class Impl>
void Context<Impl>::listen(const char *host, int port, std::function<void(Socket<Impl> *socket)> acceptHandler, std::function<Socket<Impl> *(Context *)> socketAllocator) {

    if (!socketAllocator) {
        socketAllocator = defaultSocketAllocator;
    }

    listenData.push_back({host, port, acceptHandler, socketAllocator});

    // defer the rest to impl
    Impl::listen(host, port);
}

template <class Impl>
void Context<Impl>::connect(const char *host, int port, std::function<void(Socket<Impl> *socket)> acceptHandler, std::function<Socket<Impl> *(Context *)> socketAllocator) {

    if (!socketAllocator) {
        socketAllocator = defaultSocketAllocator;
    }

    // for now
    listenData.push_back({host, port, acceptHandler, socketAllocator});
}

template <class Impl>
void Context<Impl>::run() {
    for (int i = 0; i < listenData.size(); i++) {
        listenData[i].acceptHandler(listenData[i].socketAllocator(this));
    }
}

template class Context<Berkeley<Epoll>>;

}
