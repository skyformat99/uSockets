#ifndef EPOLL_H
#define EPOLL_H

#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include <fcntl.h>
#include <chrono>
#include <algorithm>
#include <vector>

#include <iostream>

namespace uS {

typedef int SocketDescriptor;
static const int SOCKET_READABLE = EPOLLIN;
static const int SOCKET_WRITABLE = EPOLLOUT;

class Epoll
{
public:
    struct Poll;
    struct Timer;

    static void (*callbacks[16])(Poll *, int, int);

    struct Timepoint {
        void (*cb)(Timer *);
        Timer *timer;
        std::chrono::system_clock::time_point timepoint;
        int nextDelay;
    };

    int epfd;
    int numPolls = 0;
    bool cancelledLastTimer;
    int delay = -1;
    epoll_event readyEvents[1024];
    std::chrono::system_clock::time_point timepoint;
    std::vector<Timepoint> timers;
    std::vector<std::pair<Poll *, void (*)(Poll *)>> closing;

    void (*preCb)(void *) = nullptr;
    void (*postCb)(void *) = nullptr;
    void *preCbData, *postCbData;

    Epoll(bool defaultLoop) {
        epfd = epoll_create1(EPOLL_CLOEXEC);
        timepoint = std::chrono::system_clock::now();
    }

    static Epoll *createLoop(bool defaultLoop = true) {
        return new Epoll(defaultLoop);
    }

    void destroy() {
        ::close(epfd);
        delete this;
    }

    void run();

    int getEpollFd() {
        return epfd;
    }

    struct Timer {
        Epoll *loop;
        void *data;

        Timer(Epoll *loop) {
            this->loop = loop;
        }

        void start(void (*cb)(Timer *), int timeout, int repeat) {
            loop->timepoint = std::chrono::system_clock::now();
            std::chrono::system_clock::time_point timepoint = loop->timepoint + std::chrono::milliseconds(timeout);

            Timepoint t = {cb, this, timepoint, repeat};
            loop->timers.insert(
                std::upper_bound(loop->timers.begin(), loop->timers.end(), t, [](const Timepoint &a, const Timepoint &b) {
                    return a.timepoint < b.timepoint;
                }),
                t
            );

            loop->delay = -1;
            if (loop->timers.size()) {
                loop->delay = std::max<int>(std::chrono::duration_cast<std::chrono::milliseconds>(loop->timers[0].timepoint - loop->timepoint).count(), 0);
            }
        }

        void setData(void *data) {
            this->data = data;
        }

        void *getData() {
            return data;
        }

        // always called before destructor
        void stop() {
            auto pos = loop->timers.begin();
            for (Timepoint &t : loop->timers) {
                if (t.timer == this) {
                    loop->timers.erase(pos);
                    break;
                }
                pos++;
            }
            loop->cancelledLastTimer = true;

            loop->delay = -1;
            if (loop->timers.size()) {
                loop->delay = std::max<int>(std::chrono::duration_cast<std::chrono::milliseconds>(loop->timers[0].timepoint - loop->timepoint).count(), 0);
            }
        }

        void close() {
            delete this;
        }
    };

    // 4 bytes
    struct Poll {
    protected:
        struct {
            int fd : 28;
            unsigned int cbIndex : 4;
        } state = {-1, 0};

        Poll(Epoll *loop/*, uv_os_sock_t fd*/) {
            //fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
            //state.fd = fd;
            loop->numPolls++;
        }

        Poll(Poll &&other, Epoll *loop) {
            state = other.state;
            // Poll should know its own current poll!
            change(loop, this, SOCKET_READABLE);
        }

        // this is new, set fd here instead of at constructor
        void init(SocketDescriptor fd) {
            fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
            state.fd = fd;
        }

        void (*getCb())(Poll *, int, int) {
            return callbacks[state.cbIndex];
        }

        void reInit(Epoll *loop, SocketDescriptor fd) {
            state.fd = fd;
            loop->numPolls++;
        }

        void start(Epoll *loop, Poll *self, int events) {
            epoll_event event;
            event.events = events;
            event.data.ptr = self;
            epoll_ctl(loop->epfd, EPOLL_CTL_ADD, state.fd, &event);
        }

        void change(Epoll *loop, Poll *self, int events) {
            epoll_event event;
            event.events = events;
            event.data.ptr = self;
            epoll_ctl(loop->epfd, EPOLL_CTL_MOD, state.fd, &event);
        }

        void stop(Epoll *loop) {
            epoll_event event;
            epoll_ctl(loop->epfd, EPOLL_CTL_DEL, state.fd, &event);
        }

        bool fastTransfer(Epoll *loop, Epoll *newLoop, int events) {
            stop(loop);
            start(newLoop, this, events);
            loop->numPolls--;
            // needs to lock the newLoop's numPolls!
            newLoop->numPolls++;
            return true;
        }

        bool threadSafeChange(Epoll *loop, Poll *self, int events) {
            change(loop, self, events);
            return true;
        }

        void close(Epoll *loop, void (*cb)(Poll *)) {
            state.fd = -1;
            loop->closing.push_back({this, cb});
        }

    public:
        void setDerivative(int state) {
            this->state.cbIndex = state;
        }

        bool isClosed() {
            return state.fd == -1;
        }

        SocketDescriptor getFd() {
            return state.fd;
        }

        friend struct Epoll;
    };

    // this should be put in the Loop as a general "post" function always available
    struct Async : Poll {
        void (*cb)(Async *);
        Epoll *loop;
        void *data;

        Async(Epoll *loop) : Poll(loop) {
            this->loop = loop;
            this->init(::eventfd(0, EFD_CLOEXEC));
        }

        void start(void (*cb)(Async *)) {
//            this->cb = cb;
//            Poll::setCb([](Poll *p, int, int) {
//                uint64_t val;
//                if (::read(((Async *) p)->state.fd, &val, 8) == 8) {
//                    ((Async *) p)->cb((Async *) p);
//                }
//            });
//            Poll::start(loop, this, SOCKET_READABLE);
        }

        void send() {
            uint64_t one = 1;
            if (::write(state.fd, &one, 8) != 8) {
                return;
            }
        }

        void close() {
            Poll::stop(loop);
            ::close(state.fd);
            Poll::close(loop, [](Poll *p) {
                delete p;
            });
        }

        void setData(void *data) {
            this->data = data;
        }

        void *getData() {
            return data;
        }
    };
};

}

#endif // EPOLL_H
