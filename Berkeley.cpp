#include "Berkeley.h"
#include "Epoll.h"

namespace uS {

template <class Impl>
Berkeley<Impl>::Berkeley() {

}

template <class Impl>
void Berkeley<Impl>::listen(const char *host, int port) {

}

template class Berkeley<Epoll>;

}
