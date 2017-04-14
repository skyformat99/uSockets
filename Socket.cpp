#include "Socket.h"
#include "Context.h"
#include "Berkeley.h"
#include "Epoll.h"

namespace uS {

template <class Impl>
Socket<Impl>::Socket(Context<Impl> *context) : context(context) {

}

template class Socket<Berkeley<Epoll>>;

}
