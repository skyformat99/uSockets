#include "Berkeley.h"
#include "Epoll.h"

#include <iostream>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <string.h>

namespace uS {

static const SocketDescriptor SOCKET_ERROR = -1;

enum INTERNAL_STATE {
    CONNECTING = 14,
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

template <class Impl>
void Berkeley<Impl>::shutdownSocket(SocketDescriptor fd) {
#ifdef _WIN32
    shutdown(fd, SHUT_WR);
#else
    shutdown(fd, SHUT_WR);
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
void Berkeley<Impl>::setNoDelay(SocketDescriptor fd, int enable) {
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(int));
}

template <class Impl>
Berkeley<Impl>::Berkeley(Impl *impl) : impl(impl) {
    recvBuffer = new char[RECV_BUFFER_LENGTH];

    corkMessage = Socket::allocMessage(RECV_BUFFER_LENGTH);
    corkMessage->length = 0;
    corkMessage->callback = nullptr; // should be a CB that empties all enqueued CBs
}

template <class Impl>
Berkeley<Impl>::~Berkeley() {
    delete [] recvBuffer;
    Socket::freeMessage(corkMessage);
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


    ListenData listenData = {host, port, acceptHandler, socketAllocator};

    ListenPoll *listenPoll = new ListenPoll(this, listenFd, listenData);


    listenData.listenPoll = listenPoll;

    // vector of ListenPoll?
    this->listenData.push_back(listenData);

    freeaddrinfo(result);
    return true;
}

template <class Impl>
Berkeley<Impl>::ListenPoll::ListenPoll(Berkeley<Impl> *context, SocketDescriptor fd, ListenData listenData) : Impl::Poll(context->impl), context(context) {
    this->listenData = listenData;
    Impl::Poll::init(fd);

    // 15 is reserved for listening
    Impl::callbacks[INTERNAL_STATE::LISTENING] = [](typename Impl::Poll *p, int status, int events) {

        ListenPoll *listenPoll = static_cast<ListenPoll *>(p);
        Berkeley *context = listenPoll->context;

        SocketDescriptor clientFd = context->acceptSocket(listenPoll->getFd());
        if (clientFd == SOCKET_ERROR) {
            // start timer here
        } else {

            // stop timer if any

            do {
                Socket *socket = listenPoll->listenData.socketAllocator(context);

                // set FD, and start polling here
                socket->init(clientFd);
                socket->start(context->impl, socket, SOCKET_READABLE);

                listenPoll->listenData.acceptHandler(socket);
            } while ((clientFd = context->acceptSocket(listenPoll->getFd())) != SOCKET_ERROR);
        }
    };

    Impl::Poll::setDerivative(INTERNAL_STATE::LISTENING);
    Impl::Poll::start(context->impl, this, SOCKET_READABLE);
}

template <class Impl>
void Berkeley<Impl>::stopListening() {
    for (ListenData &listenData : this->listenData) {
        listenData.listenPoll->close();
    }
}

template <class Impl>
void Berkeley<Impl>::ListenPoll::close() {
    SocketDescriptor fd = Impl::Poll::getFd();
    Impl::Poll::close(context->impl, [](typename Impl::Poll *poll) {
        delete static_cast<ListenPoll *>(poll);
    });
    context->closeSocket(fd);
}

template <class Impl>
void Berkeley<Impl>::connect(const char *host, int port, std::function<void(Socket *socket)> connectionHandler, std::function<Socket *(Berkeley *)> socketAllocator) {

    if (!socketAllocator) {
        socketAllocator = defaultSocketAllocator;
    }

    addrinfo hints, *result;
    memset(&hints, 0, sizeof(addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host, std::to_string(port).c_str(), &hints, &result) != 0) {
        connectionHandler(nullptr);
        return;
    }

    SocketDescriptor fd = createSocket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (fd == SOCKET_ERROR) {
        freeaddrinfo(result);
        connectionHandler(nullptr);
        return;
    }

    ::connect(fd, result->ai_addr, result->ai_addrlen);
    freeaddrinfo(result);

    Socket *socket = socketAllocator(this);
    socket->init(fd);

    socket->userData = new std::function<void(Socket *socket)>(connectionHandler);
    Impl::callbacks[INTERNAL_STATE::CONNECTING] = [](typename Impl::Poll *p, int status, int events) {
        Socket *socket = (Socket *) p;
        socket->change(socket->context->impl, socket, SOCKET_READABLE);
        std::function<void(Socket *socket)> *f = (std::function<void(Socket *socket)> *) socket->userData;
        (*f)(socket);
        delete f;
    };

    socket->setDerivative(INTERNAL_STATE::CONNECTING);
    socket->start(this->impl, socket, SOCKET_WRITABLE);
}

template <class Impl>
void Berkeley<Impl>::Socket::close(void (*cb)(Socket *)) {
    SocketDescriptor fd = Impl::Poll::getFd();

    Impl::Poll::stop(context->impl);
    Impl::Poll::close(context->impl, (void (*)(typename Impl::Poll *)) cb);

    context->closeSocket(fd);
}

template <class Impl>
void Berkeley<Impl>::ioHandler(Socket *(*onData)(Socket *, char *, size_t), void (*onEnd)(Socket *), Socket *socket, int status, int events) {

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

template <class Impl>
void Berkeley<Impl>::Socket::cork(bool enable) {

    //std::cout << "Cork: " << enable << std::endl;

    if (enable == corked) {
        std::cout << "ERROR: Cork did not change setting!" << std::endl;
        //std::terminate();
        return;
    }

    corked = enable;
    if (corked) {
        context->corkMessage->length = 0;
    } else if (context->corkMessage->length) {
        sendMessage(context->corkMessage, false);
    }
}

template <class Impl>
void Berkeley<Impl>::Socket::setNoDelay(bool enable) {
    context->setNoDelay(Impl::Poll::getFd(), enable);
}

template <class Impl>
typename Berkeley<Impl>::Socket::Queue::Message *Berkeley<Impl>::Socket::allocMessage(size_t length, const char *data) {
    typename Queue::Message *messagePtr = (typename Queue::Message *) new char[sizeof(typename Queue::Message) + length];
    messagePtr->length = length;
    messagePtr->data = ((char *) messagePtr) + sizeof(typename Queue::Message);
    messagePtr->nextMessage = nullptr;

    if (data) {
        memcpy((char *) messagePtr->data, data, messagePtr->length);
    }

    return messagePtr;
}

template <class Impl>
bool Berkeley<Impl>::Socket::sendMessage(typename Queue::Message *message, bool moveMessage) {

    if (corked) {


        // if length is outside, send cork buffer and restart

        memcpy(context->corkMessage->data + context->corkMessage->length, message->data, message->length);
        context->corkMessage->length += message->length;

        // also register this Queue's callback and callback data into the context's cork-vector

        // the cork buffer is in itself a Message, which has its own callback with callbackdata
        // set to point at the shared vector of other (corked) message callbacks to be called
        // once this cork buffers callback is called
        // the cork buffer can be sent with this function

        return true;
    }

    if (messageQueue.empty()) {

        //std::cout << "Sending on fd: " << this->getFd() << " " << std::string(message->data, message->length) << std::endl;

        ssize_t sent = ::send(this->getFd(), message->data, message->length, MSG_NOSIGNAL);
        if (sent == (ssize_t) message->length) {
            return true;
        } else if (sent == SOCKET_ERROR) {
            if (!context->wouldBlock()) {
                if (message->callback) {
                    message->callback(this, message->callbackData, true, nullptr);
                }
                return true;
            }
        } else {
            message->length -= sent;
            message->data += sent;
        }
    }

    if (moveMessage) {
        messageQueue.push(message);
    } else {
        // we need to copy the remains to a new buffer and queue it
        // used for big buffers like the cork buffer!

    }

    // at this point, update poll
//    if ((getPoll() & SOCKET_WRITABLE) == 0) {
//        setPoll(getPoll() | SOCKET_WRITABLE);
//        //changePoll(this);
//    }

    return false;
}

template <class Impl>
bool Berkeley<Impl>::Socket::isShuttingDown() {
    return state.shuttingDown;
}

// should empty the queue and append optional data and set shutting down appropriately
template <class Impl>
void Berkeley<Impl>::Socket::shutdown() {
    context->shutdownSocket(Impl::Poll::getFd());
}

template class Berkeley<Epoll>;

}
