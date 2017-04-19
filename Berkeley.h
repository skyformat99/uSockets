#ifndef BERKELEY_H
#define BERKELEY_H

#include <functional>
#include <vector>
#include <iostream>

namespace uS {

typedef int SocketDescriptor;

enum {
    ONLY_IPV4,
    REUSE_PORT
};

template <class Impl>
class Berkeley : public Impl { // it should HAVE a Impl, not derive from it! Take Impl ptr as constructor
public:
    class Socket : public Impl::Poll { //SocketBase
    public:
        struct Queue {
            struct Message {
                char *data;
                size_t length;
                Message *nextMessage = nullptr;
                void (*callback)(Socket *socket, void *data, bool cancelled, void *reserved) = nullptr;
                void *callbackData = nullptr, *reserved = nullptr;
            };

            Message *head = nullptr, *tail = nullptr;
            void pop()
            {
                Message *nextMessage;
                if ((nextMessage = head->nextMessage)) {
                    delete [] (char *) head;
                    head = nextMessage;
                } else {
                    delete [] (char *) head;
                    head = tail = nullptr;
                }
            }

            bool empty() {return head == nullptr;}
            Message *front() {return head;}

            void push(Message *message)
            {
                message->nextMessage = nullptr;
                if (tail) {
                    tail->nextMessage = message;
                    tail = message;
                } else {
                    head = message;
                    tail = message;
                }
            }
        } messageQueue;

        using Message = typename Queue::Message;

    protected:
        // 4 byte here
        Berkeley *context;
        void *userData;

        bool corked = false;

        // helpers
        static typename Queue::Message *allocMessage(size_t length, const char *data = nullptr);
        void freeMessage(typename Queue::Message *message) {
            delete [] (char *) message;
        }

    public:
        Socket(Berkeley *context) : context(context), Impl::Poll(context) {

        }

        Berkeley *getContext() {
            return context;
        }

        void shutdown();
        void close(void (*cb)(Socket *));
        bool isShuttingDown();
        void setUserData(void *userData) {
            this->userData = userData;
        }

        void *getUserData() {
            return userData;
        }

        bool sendMessage(typename Queue::Message *message, bool moveMessage = true);

        // higher level send function good for framing other higher level protocols
        // sendTransformed

        void cork(bool enable);

        friend class Berkeley;
    };

private:

    // these can be passed as arugment from Hub
    char *recvBuffer;
    typename Socket::Message *corkMessage;

    // helper functions
    SocketDescriptor createSocket(int, int, int);
    SocketDescriptor acceptSocket(int);
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
    static inline void ioHandler(void (*onData)(Socket *, char *, size_t), void (*onEnd)(Socket *), Socket *, int, int);

public:
    Berkeley();

    template <class State>
    void registerSocketDerivative(int index) {
        // todo: move this vector to Berkeley so that libuv can also implement this
        Impl::callbacks[index] = [](typename Impl::Poll *poll, int status, int events) {
            ioHandler(State::onData, State::onEnd, (Socket *) poll, status, events);
        };
    }

    bool listen(const char *host, int port, int options, std::function<void(Socket *socket)> acceptHandler, std::function<Socket *(Berkeley *)> socketAllocator = nullptr);
    void connect(const char *host, int port, std::function<void(Socket *socket)> connectionHandler, std::function<Socket *(Berkeley *)> socketAllocator = nullptr);
};

}

#endif // BERKELEY_H
