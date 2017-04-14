#ifndef SOCKET_H
#define SOCKET_H

namespace uS {

template <class Impl> class Context;

template <class Impl>
class Socket {
    Context<Impl> *context;
public:
    Socket(Context<Impl> *context);
};

}

#endif // SOCKET_H
