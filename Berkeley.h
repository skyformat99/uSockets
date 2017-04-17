#ifndef BERKELEY_H
#define BERKELEY_H

#include <functional>
#include <vector>
#include <iostream>

namespace uS {

typedef int SocketDescriptor;
static const SocketDescriptor SOCKET_ERROR = -1;

enum {
    ONLY_IPV4,
    REUSE_PORT
};

template <class Impl>
class Berkeley : public Impl {
public:
    class Socket : public Impl::Poll {
        Berkeley *context;

    public:
        Socket(Berkeley *context) : context(context), Impl::Poll(context) {

        }

        Berkeley *getContext() {
            return context;
        }

        void shutdown();
        void close();

        friend class Berkeley;
    };

private:

    char *recvBuffer;

    // helper functions
    SocketDescriptor createSocket(int, int, int);
    SocketDescriptor acceptSocket(int);
    bool wouldBlock();
    void closeSocket(SocketDescriptor fd);

    std::function<Socket *(Berkeley *)> defaultSocketAllocator;

    // this data is similar to what you pass to listen, maybe let the user fill it and have different helper constructors?
    struct ListenData {
        const char *host;
        int port;
        std::function<void(Socket *socket)> acceptHandler;
        std::function<Socket *(Berkeley *)> socketAllocator;

        typename Impl::Poll *listenPoll;
    };

    std::vector<ListenData> listenData;
    static void ioHandler(void (*onData)(Socket *, char *, size_t), void (*onEnd)(Socket *), Socket *, int, int);

public:
    Berkeley();

    template <class State>
    void addSocketState(int index) {
        struct PollHandler {
            static void f(typename Impl::Poll *poll, int status, int events) {
                ioHandler(State::onData, State::onEnd, (Socket *) poll, status, events);
            }
        };

        // todo: move this vector to Berkeley so that libuv can also implement this
        Impl::callbacks[index] = PollHandler::f;
    }

    bool listen(const char *host, int port, int options, std::function<void(Socket *socket)> acceptHandler, std::function<Socket *(Berkeley *)> socketAllocator = nullptr);
    void connect(const char *host, int port, std::function<void(Socket *socket)> acceptHandler, std::function<Socket *(Berkeley *)> socketAllocator = nullptr);

};

}

#endif // BERKELEY_H
