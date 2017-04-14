#ifndef CONTEXT_H
#define CONTEXT_H

#include "Socket.h"

#include <functional>
#include <vector>

namespace uS {

template <class Impl>
class Context : Impl {

    std::function<Socket<Impl> *(Context *)> defaultSocketAllocator;

    // this data is similar to what you pass to listen, maybe let the user fill it and have different helper constructors?
    struct ListenData {
        const char *host;
        int port;
        std::function<void(Socket<Impl> *socket)> acceptHandler;
        std::function<Socket<Impl> *(Context *)> socketAllocator;
    };

    std::vector<ListenData> listenData;
public:
    Context();

    // implemented by the impl
    void listen(const char *host, int port, std::function<void(Socket<Impl> *socket)> acceptHandler, std::function<Socket<Impl> *(Context *)> socketAllocator = nullptr);
    void connect(const char *host, int port, std::function<void(Socket<Impl> *socket)> acceptHandler, std::function<Socket<Impl> *(Context *)> socketAllocator = nullptr);

    // pre, post events
    //

    void run();
};

}

#endif // CONTEXT_H
