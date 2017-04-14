#include "Context.h"

namespace uS {

Context::Context() {
    defaultSocketAllocator = [](Context *context) {
        return new Socket(context);
    };
}

void Context::listen(const char *host, int port, std::function<void(Socket *socket)> acceptHandler, std::function<Socket *(Context *)> socketAllocator) {

    if (!socketAllocator) {
        socketAllocator = defaultSocketAllocator;
    }

    listenData.push_back({host, port, acceptHandler, socketAllocator});

    // defer the rest to impl
    impl.listen(host, port);
}

void Context::connect(const char *host, int port, std::function<void(Socket *socket)> acceptHandler, std::function<Socket *(Context *)> socketAllocator) {

    if (!socketAllocator) {
        socketAllocator = defaultSocketAllocator;
    }

    // for now
    listenData.push_back({host, port, acceptHandler, socketAllocator});
}

void Context::run() {
    for (int i = 0; i < listenData.size(); i++) {
        listenData[i].acceptHandler(listenData[i].socketAllocator(this));
    }
}

}
