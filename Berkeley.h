#ifndef BERKELEY_H
#define BERKELEY_H

#include <functional>
#include <vector>

namespace uS {

typedef int SocketDescriptor;
static const SocketDescriptor INVALID_SOCKET = -1;

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
    SocketDescriptor createSocket();
    SocketDescriptor acceptSocket();
    bool wouldBlock();
    void closeSocket();

    std::function<Socket *(Berkeley *)> defaultSocketAllocator;

    // this data is similar to what you pass to listen, maybe let the user fill it and have different helper constructors?
    struct ListenData {
        const char *host;
        int port;
        std::function<void(Socket *socket)> acceptHandler;
        std::function<Socket *(Berkeley *)> socketAllocator;
    };

    std::vector<ListenData> listenData;


public:
    Berkeley();

    void listen(const char *host, int port, std::function<void(Socket *socket)> acceptHandler, std::function<Socket *(Berkeley *)> socketAllocator = nullptr);
    void connect(const char *host, int port, std::function<void(Socket *socket)> acceptHandler, std::function<Socket *(Berkeley *)> socketAllocator = nullptr);


    // can be implemented in Impl
    //void run();

};

}

#endif // BERKELEY_H
