#ifndef BERKELEY_H
#define BERKELEY_H

// Top-level cross-platform BSD-like socket implementation
// Berkeley object is the inner context (mtcp_context)

namespace uS {

typedef int SocketDescriptor;
static const SocketDescriptor INVALID_SOCKET = -1;

template <class Impl>
class Berkeley : Impl {

    // helper functions
    SocketDescriptor createSocket();
    SocketDescriptor acceptSocket();
    bool wouldBlock();
    void closeSocket();


public:
    Berkeley();

    void listen(const char *host, int port);


};

}

#endif // BERKELEY_H
