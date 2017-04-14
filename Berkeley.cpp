#include "Berkeley.h"
#include "Epoll.h"

#include <iostream>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>

namespace uS {

template <class Impl>
SocketDescriptor Berkeley<Impl>::createSocket(int domain, int type, int protocol) {
    // returns INVALID_SOCKET on error
    int flags = 0;
#if defined(SOCK_CLOEXEC) && defined(SOCK_NONBLOCK)
    flags = SOCK_CLOEXEC | SOCK_NONBLOCK;
#endif

    uv_os_sock_t createdFd = socket(domain, type | flags, protocol);

#ifdef __APPLE__
    if (createdFd != INVALID_SOCKET) {
        int noSigpipe = 1;
        setsockopt(createdFd, SOL_SOCKET, SO_NOSIGPIPE, &noSigpipe, sizeof(int));
    }
#endif

    return createdFd;
}

template <class Impl>
void Berkeley<Impl>::closeSocket(SocketDescriptor fd) {
#ifdef _WIN32
    closesocket(fd);
#else
    close(fd);
#endif
}

template <class Impl>
Berkeley<Impl>::Berkeley() : Impl(true) {

}

template <class Impl>
bool Berkeley<Impl>::listen(const char *host, int port, int options, std::function<void(Socket *socket)> acceptHandler, std::function<Socket *(Berkeley *)> socketAllocator) {

    if (!socketAllocator) {
        socketAllocator = defaultSocketAllocator;
    }

    addrinfo hints, *result;
    memset(&hints, 0, sizeof(addrinfo));

    hints.ai_flags = AI_PASSIVE;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(host, std::to_string(port).c_str(), &hints, &result)) {
        return false;
    }

    uv_os_sock_t listenFd = SOCKET_ERROR;
    addrinfo *listenAddr;
    if ((options & ONLY_IPV4) == 0) {
        for (addrinfo *a = result; a && listenFd == SOCKET_ERROR; a = a->ai_next) {
            if (a->ai_family == AF_INET6) {
                listenFd = createSocket(a->ai_family, a->ai_socktype, a->ai_protocol);
                listenAddr = a;
            }
        }
    }

    for (addrinfo *a = result; a && listenFd == SOCKET_ERROR; a = a->ai_next) {
        if (a->ai_family == AF_INET) {
            listenFd = createSocket(a->ai_family, a->ai_socktype, a->ai_protocol);
            listenAddr = a;
        }
    }

    if (listenFd == SOCKET_ERROR) {
        freeaddrinfo(result);
        return false;
    }

#ifdef __linux
#ifdef SO_REUSEPORT
    if (options & REUSE_PORT) {
        int optval = 1;
        setsockopt(listenFd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
    }
#endif
#endif

    int enabled = true;
    setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &enabled, sizeof(enabled));

    if (bind(listenFd, listenAddr->ai_addr, listenAddr->ai_addrlen) || ::listen(listenFd, 512)) {
        closeSocket(listenFd);
        freeaddrinfo(result);
        return false;
    }

    // listenSocket?
    class ListenPoll : public Impl::Poll {
    public:
        ListenPoll(Impl *impl, SocketDescriptor fd) : Impl::Poll(impl, fd) {
            Impl::Poll::setCb([](typename Impl::Poll *p, int status, int events) {


                // ListenPoll from p

//                uv_os_sock_t serverFd = listenSocket->getFd();
//                Context *netContext = listenSocket->nodeData->netContext;
//                uv_os_sock_t clientFd = netContext->acceptSocket(serverFd);
//                if (clientFd == INVALID_SOCKET) {
//                    /*
//                    * If accept is failing, the pending connection won't be removed and the
//                    * polling will cause the server to spin, using 100% cpu. Switch to a timer
//                    * event instead to avoid this.
//                    */
//                    if (!TIMER && !netContext->wouldBlock()) {
//                        listenSocket->stop(listenSocket->nodeData->loop);

//                        listenSocket->timer = new Timer(listenSocket->nodeData->loop);
//                        listenSocket->timer->setData(listenSocket);
//                        listenSocket->timer->start(accept_timer_cb<A>, 1000, 1000);
//                    }
//                    return;
//                } else if (TIMER) {
//                    listenSocket->timer->stop();
//                    listenSocket->timer->close();
//                    listenSocket->timer = nullptr;

//                    listenSocket->setCb(accept_poll_cb<A>);
//                    listenSocket->start(listenSocket->nodeData->loop, listenSocket, UV_READABLE);
//                }
//                do {
//                    SSL *ssl = nullptr;
//                    if (listenSocket->sslContext) {
//                        ssl = SSL_new(listenSocket->sslContext.getNativeContext());
//                        SSL_set_accept_state(ssl);
//                    }

//                    Socket *socket = new Socket(listenSocket->nodeData, listenSocket->nodeData->loop, clientFd, ssl);
//                    socket->setPoll(UV_READABLE);
//                    A(socket);
//                } while ((clientFd = netContext->acceptSocket(serverFd)) != INVALID_SOCKET);




            });
            Impl::Poll::start(impl, this, UV_READABLE);
        }
    };

    ListenPoll *listenPoll = new ListenPoll(this, listenFd);

    // vector of ListenPoll?
    listenData.push_back({host, port, acceptHandler, socketAllocator, listenPoll});

    freeaddrinfo(result);
    return true;
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
