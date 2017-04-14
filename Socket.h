#ifndef SOCKET_H
#define SOCKET_H

namespace uS {

class Context;

class Socket {
    Context *context;
public:
    Socket(Context *context);
};

}

#endif // SOCKET_H
