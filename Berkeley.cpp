#include "Berkeley.h"
#include "Epoll.h"

#include <iostream>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>

namespace uS {

enum INTERNAL_STATE {
    LISTENING = 15
};

static const int RECV_BUFFER_LENGTH = 1024 * 300;

template <class Impl>
SocketDescriptor Berkeley<Impl>::createSocket(int domain, int type, int protocol) {
    // returns INVALID_SOCKET on error
    int flags = 0;
#if defined(SOCK_CLOEXEC) && defined(SOCK_NONBLOCK)
    flags = SOCK_CLOEXEC | SOCK_NONBLOCK;
#endif

    SocketDescriptor createdFd = socket(domain, type | flags, protocol);

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

// returns INVALID_SOCKET on error
template <class Impl>
SocketDescriptor Berkeley<Impl>::acceptSocket(SocketDescriptor fd) {
    SocketDescriptor acceptedFd;
#if defined(SOCK_CLOEXEC) && defined(SOCK_NONBLOCK)
    // Linux, FreeBSD
    acceptedFd = accept4(fd, nullptr, nullptr, SOCK_CLOEXEC | SOCK_NONBLOCK);
#else
    // Windows, OS X
    acceptedFd = accept(fd, nullptr, nullptr);
#endif

#ifdef __APPLE__
    if (acceptedFd != INVALID_SOCKET) {
        int noSigpipe = 1;
        setsockopt(acceptedFd, SOL_SOCKET, SO_NOSIGPIPE, &noSigpipe, sizeof(int));
    }
#endif
    return acceptedFd;
}

template <class Impl>
bool Berkeley<Impl>::wouldBlock() {
#ifdef _WIN32
    return WSAGetLastError() == WSAEWOULDBLOCK;
#else
    return errno == EWOULDBLOCK;// || errno == EAGAIN;
#endif
}

template <class Impl>
Berkeley<Impl>::Berkeley() : Impl(true) {
    recvBuffer = new char[RECV_BUFFER_LENGTH];
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

    SocketDescriptor listenFd = SOCKET_ERROR;
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

    class ListenPoll : public Impl::Poll {
        ListenData listenData;
        Impl *impl;
    public:
        ListenPoll(Impl *impl, SocketDescriptor fd, ListenData listenData) : Impl::Poll(impl) {
            this->listenData = listenData;
            this->impl = impl;
            Impl::Poll::init(fd);

            // 15 is reserved for listening
            Impl::callbacks[INTERNAL_STATE::LISTENING] = [](typename Impl::Poll *p, int status, int events) {

                ListenPoll *listenPoll = static_cast<ListenPoll *>(p);
                Berkeley *context = static_cast<Berkeley *>(listenPoll->impl);

                SocketDescriptor clientFd = context->acceptSocket(listenPoll->getFd());
                if (clientFd == SOCKET_ERROR) {
                    // start timer here
                } else {

                    // stop timer if any

                    do {
                        Socket *socket = listenPoll->listenData.socketAllocator(context);

                        // set FD, and start polling here
                        socket->init(clientFd);
                        socket->start(context, socket, SOCKET_READABLE);

                        listenPoll->listenData.acceptHandler(socket);
                    } while ((clientFd = context->acceptSocket(listenPoll->getFd())) != SOCKET_ERROR);
                }
            };

            Impl::Poll::setState(INTERNAL_STATE::LISTENING);
            Impl::Poll::start(impl, this, SOCKET_READABLE);
        }
    };


    ListenData listenData = {host, port, acceptHandler, socketAllocator};

    ListenPoll *listenPoll = new ListenPoll(this, listenFd, listenData);



    // vector of ListenPoll?
    this->listenData.push_back(listenData);

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
    context->closeSocket(Impl::Poll::getFd());
}

template <class Impl>
void Berkeley<Impl>::ioHandler(void (*onData)(Socket *, char *, size_t), void (*onEnd)(Socket *), Socket *socket, int status, int events) {

    Berkeley<Impl> *context = socket->context;

    if (status < 0) {
        onEnd(socket);
        return;
    }

//    if (events & SOCKET_WRITABLE) {
//        if (!socket->messageQueue.empty() && (events & SOCKET_WRITABLE)) {
//            socket->cork(true);
//            while (true) {
//                Queue::Message *messagePtr = socket->messageQueue.front();
//                ssize_t sent = ::send(socket->getFd(), messagePtr->data, messagePtr->length, MSG_NOSIGNAL);
//                if (sent == (ssize_t) messagePtr->length) {
//                    if (messagePtr->callback) {
//                        messagePtr->callback(p, messagePtr->callbackData, false, messagePtr->reserved);
//                    }
//                    socket->messageQueue.pop();
//                    if (socket->messageQueue.empty()) {
//                        // todo, remove bit, don't set directly
//                        socket->change(socket->nodeData->loop, socket, socket->setPoll(UV_READABLE));
//                        break;
//                    }
//                } else if (sent == SOCKET_ERROR) {
//                    if (!netContext->wouldBlock()) {
//                        STATE::onEnd((Socket *) p);
//                        return;
//                    }
//                    break;
//                } else {
//                    messagePtr->length -= sent;
//                    messagePtr->data += sent;
//                    break;
//                }
//            }
//            socket->cork(false);
//        }
//    }

    if (events & SOCKET_READABLE) {
        int length = recv(socket->getFd(), context->recvBuffer, RECV_BUFFER_LENGTH, 0);
        if (length > 0) {
            onData(socket, context->recvBuffer, length);
        } else if (!length || (length == SOCKET_ERROR && !context->wouldBlock())) {
            onEnd(socket);
        }
    }
}

template class Berkeley<Epoll>;

}
