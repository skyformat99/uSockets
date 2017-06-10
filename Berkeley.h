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
class Berkeley {

    Impl *impl;

public:

    Impl *getLoop() {
        return impl;
    }

    class Socket : public Impl::Poll { //SocketBase

        // should also hold corked!
        struct {
            int poll : 4;
            int shuttingDown : 4;
        } state = {0, false};

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
        Berkeley *context;
        void *userData;

        // should lie in state!
        bool corked = false;

        // helpers
        static typename Queue::Message *allocMessage(size_t length, const char *data = nullptr);
        static void freeMessage(typename Queue::Message *message) {
            delete [] (char *) message;
        }

    public:
        Socket(Berkeley *context) : Impl::Poll(context->impl), context(context) {
            setNoDelay(true);
        }

        Socket(Socket &&other) : Impl::Poll(std::move(other), other.context->impl) {
            context = other.context;
            userData = other.userData;
            corked = other.corked;
            next = other.next;
            prev = other.prev;
        }

        Berkeley *getContext() {
            return context;
        }

        void setNoDelay(bool enable);
        void close(void (*cb)(Socket *));

        // shutdown should probably automatically set isShutting down, and pass optional data and shut down when the queue is empty
        void shutdown();
        void setShuttingDown(bool shuttingDown) {
            state.shuttingDown = shuttingDown;
        }

        bool isShuttingDown();
        void setUserData(void *userData) {
            this->userData = userData;
        }

        void *getUserData() {
            return userData;
        }

        bool sendMessage(typename Queue::Message *message, bool moveMessage = true);
        void cork(bool enable);

        // these can possibly live in each derivative
        Socket *next = nullptr, *prev = nullptr;

        friend class Berkeley;








        template <class T, class D>
        void sendTransformed(const char *message, size_t length, void(*callback)(void *socket, void *data, bool cancelled, void *reserved), void *callbackData, D transformData) {
            size_t estimatedLength = T::estimate(message, length) + sizeof(typename Queue::Message);

            // allokera estimatedLength från corkbuffern

            //typename Queue::Message *messagePtr = allocMessage(estimatedLength - sizeof(typename Queue::Message));

retry:

            if (corked && estimatedLength < 300 * 1024) {

                if (context->corkMessage->length + estimatedLength < 300 * 1024) {

                    typename Queue::Message m;
                    typename Queue::Message *messagePtr = &m;

                    messagePtr->data = context->corkMessage->data + context->corkMessage->length;

                    messagePtr->length = T::transform(message, (char *) messagePtr->data, length, transformData);

                    context->corkMessage->length += messagePtr->length;
                    messagePtr->callback = nullptr;

                    //std::cout << "Cork buffer length: " << context->corkMessage->length << std::endl;

                    // these are wrong (vector of callbacks)
                    context->corkMessage->callback = (void (*)(Socket *, void *, bool, void *)) callback;
                    context->corkMessage->callbackData = callbackData;

                } else {

                    std::cout << "Sheet!" << std::endl;
                    cork(false);
                    cork(true);
                    goto retry;


                }

            } else {

                // MÅSTE IMPLEMENTERA ICKE-KORK för broadcast!

                typename Queue::Message m;
                m.callback = (void (*)(Socket *, void *, bool, void *)) callback;
                m.callbackData = callbackData;

                // använd en temp-buffer för att formatera på!
                m.data = new char[estimatedLength];

                m.length = T::transform(message, (char *) m.data, length, transformData);

                sendMessage(&m, false);

                delete [] m.data;

            }


//            if (hasEmptyQueue()) {
//                if (estimatedLength <= uS::NodeData::preAllocMaxSize) {
//                    int memoryLength = estimatedLength;
//                    int memoryIndex = nodeData->getMemoryBlockIndex(memoryLength);

//                    Queue::Message *messagePtr = (Queue::Message *) nodeData->getSmallMemoryBlock(memoryIndex);
//                    messagePtr->data = ((char *) messagePtr) + sizeof(Queue::Message);
//                    messagePtr->length = T::transform(message, (char *) messagePtr->data, length, transformData);

//                    bool wasTransferred;
//                    if (write(messagePtr, wasTransferred)) {
//                        if (!wasTransferred) {
//                            nodeData->freeSmallMemoryBlock((char *) messagePtr, memoryIndex);
//                            if (callback) {
//                                callback(this, callbackData, false, nullptr);
//                            }
//                        } else {
//                            messagePtr->callback = callback;
//                            messagePtr->callbackData = callbackData;
//                        }
//                    } else {
//                        nodeData->freeSmallMemoryBlock((char *) messagePtr, memoryIndex);
//                        if (callback) {
//                            callback(this, callbackData, true, nullptr);
//                        }
//                    }
//                } else {
//                    Queue::Message *messagePtr = allocMessage(estimatedLength - sizeof(Queue::Message));
//                    messagePtr->length = T::transform(message, (char *) messagePtr->data, length, transformData);

//                    bool wasTransferred;
//                    if (write(messagePtr, wasTransferred)) {
//                        if (!wasTransferred) {
//                            freeMessage(messagePtr);
//                            if (callback) {
//                                callback(this, callbackData, false, nullptr);
//                            }
//                        } else {
//                            messagePtr->callback = callback;
//                            messagePtr->callbackData = callbackData;
//                        }
//                    } else {
//                        freeMessage(messagePtr);
//                        if (callback) {
//                            callback(this, callbackData, true, nullptr);
//                        }
//                    }
//                }
//            } else {
//                Queue::Message *messagePtr = allocMessage(estimatedLength - sizeof(Queue::Message));
//                messagePtr->length = T::transform(message, (char *) messagePtr->data, length, transformData);
//                messagePtr->callback = callback;
//                messagePtr->callbackData = callbackData;
//                enqueue(messagePtr);
//            }
        }





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
    void shutdownSocket(SocketDescriptor fd);
    void setNoDelay(SocketDescriptor fd, int enable);

    std::function<Socket *(Berkeley *)> defaultSocketAllocator;

    class ListenPoll;

    // this data is similar to what you pass to listen, maybe let the user fill it and have different helper constructors?
    struct ListenData {
        const char *host;
        int port;
        std::function<void(Socket *socket)> acceptHandler;
        std::function<Socket *(Berkeley *)> socketAllocator;

        ListenPoll *listenPoll;
    };

    class ListenPoll : public Impl::Poll {
        ListenData listenData;
        Berkeley<Impl> *context;
    public:
        ListenPoll(Berkeley<Impl> *context, SocketDescriptor fd, ListenData listenData);
        void close();
    };

    std::vector<ListenData> listenData;
    static inline void ioHandler(Socket *(*onData)(Socket *, char *, size_t), void (*onEnd)(Socket *), Socket *, int, int);

public:
    Berkeley(Impl *impl);
    ~Berkeley();

    template <class State>
    void registerSocketDerivative(int index) {
        // todo: move this vector to Berkeley so that libuv can also implement this
        Impl::callbacks[index] = [](typename Impl::Poll *poll, int status, int events) {
            ioHandler((Socket *(*)(Socket *, char *, size_t)) State::onData, (void (*)(Socket *)) State::onEnd, (Socket *) poll, status, events);
        };
    }

    bool listen(const char *host, int port, int options, std::function<void(Socket *socket)> acceptHandler, std::function<Socket *(Berkeley *)> socketAllocator = nullptr);
    void connect(const char *host, int port, std::function<void(Socket *socket)> connectionHandler, std::function<Socket *(Berkeley *)> socketAllocator = nullptr);

    void stopListening();
};

}

#endif // BERKELEY_H
