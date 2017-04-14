#ifndef CONTEXT_H
#define CONTEXT_H

#include "Socket.h"

#include <functional>
#include <vector>

// interface implemented in layers with different backends
// BSD wrapper (top level) has poll backends (lowest level)
// TCP stack has everything in one module

// Context<BSD<Epoll>>
// Context<BSD<Libuv>>
// Context<BSD<Asio>>
// Context<uSockets>

// hard-code BSD for now
#include "Berkeley.h"

namespace uS {

class Context {

    Berkeley impl;

    std::function<Socket *(Context *)> defaultSocketAllocator;

    // this data is similar to what you pass to listen, maybe let the user fill it and have different helper constructors?
    struct ListenData {
        const char *host;
        int port;
        std::function<void(Socket *socket)> acceptHandler;
        std::function<Socket *(Context *)> socketAllocator;
    };

    std::vector<ListenData> listenData;
public:
    Context();

    // implemented by the impl
    void listen(const char *host, int port, std::function<void(Socket *socket)> acceptHandler, std::function<Socket *(Context *)> socketAllocator = nullptr);
    void connect(const char *host, int port, std::function<void(Socket *socket)> acceptHandler, std::function<Socket *(Context *)> socketAllocator = nullptr);

    // pre, post events
    //

    void run();
};

}

#endif // CONTEXT_H
