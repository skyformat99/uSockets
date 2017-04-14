#ifndef BERKELEY_H
#define BERKELEY_H

#include <functional>
#include <vector>

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
    class Socket : Impl::Poll {
        Berkeley *context;

    public:
        Socket(Berkeley *context) : context(context), Impl::Poll(context, 0) {

        }

        Berkeley *getContext() {
            return context;
        }

        void shutdown();
        void close();
    };
private:

    // helper functions
    SocketDescriptor createSocket(int, int, int);
    SocketDescriptor acceptSocket();
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


public:
    Berkeley();

    bool listen(const char *host, int port, int options, std::function<void(Socket *socket)> acceptHandler, std::function<Socket *(Berkeley *)> socketAllocator = nullptr);
    void connect(const char *host, int port, std::function<void(Socket *socket)> acceptHandler, std::function<Socket *(Berkeley *)> socketAllocator = nullptr);

};

}

#endif // BERKELEY_H
