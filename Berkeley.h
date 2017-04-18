#ifndef BERKELEY_H
#define BERKELEY_H

#include <functional>
#include <vector>

namespace uS {

typedef int SocketDescriptor;

enum {
    ONLY_IPV4,
    REUSE_PORT
};

template <class Impl>
class Berkeley : public Impl {
public:
    class Socket : public Impl::Poll {
        // 4 byte here
        Berkeley *context;
        void *userData;

    public:
        Socket(Berkeley *context) : context(context), Impl::Poll(context) {

        }

        Berkeley *getContext() {
            return context;
        }

        void send(const char *data, size_t length, void (*cb)(Socket *, bool cancelled) = nullptr);
        void shutdown();
        void close(void (*cb)(Socket *));
        bool isShuttingDown();
        void cork(bool enable);
        void setUserData(void *userData) {
            this->userData = userData;
        }

        void *getUserData() {
            return userData;
        }

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
    static inline void ioHandler(void (*onData)(Socket *, char *, size_t), void (*onEnd)(Socket *), Socket *, int, int);

public:
    Berkeley();

    template <class State>
    void registerSocketDerivative(int index) {
        // todo: move this vector to Berkeley so that libuv can also implement this
        Impl::callbacks[index] = [](typename Impl::Poll *poll, int status, int events) {
            ioHandler(State::onData, State::onEnd, (Socket *) poll, status, events);
        };
    }

    bool listen(const char *host, int port, int options, std::function<void(Socket *socket)> acceptHandler, std::function<Socket *(Berkeley *)> socketAllocator = nullptr);
    void connect(const char *host, int port, std::function<void(Socket *socket)> connectionHandler, std::function<Socket *(Berkeley *)> socketAllocator = nullptr);

};

}

#endif // BERKELEY_H
