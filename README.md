## µSockets

µS is a lightweight & speedy cross-platform TCP library with composable implementation layers **under development in 2018**.

#### Why
A separation between eventing (Epoll, Kqueue, libuv, etc) and networking (BSD, WinSock) allows it to fit in everywhere via tiny eventing plugins or complete replacements. µWebSockets needs to be properly split up into (static) modules to ease maintenance and addition of new features.

Planned layers:

* Socket layers: BSD, WinSock, (mTCP, µSockets)
* Event layers: Epoll, Kqueue, Libuv, (ASIO)

Design goals:

* Minimal, easy-to-use, static, no-bullshit, "async"-only, symmetric interface.
* Optimized small-message, big-message, send (and broadcasting?).
* Very lightweight & speedy (to be next-gen µWebSockets foundation).
* Can be made to fit in almost everywhere via layered design.
* (Might) get its own experimental user-space TCP stack (ongoing project).

Perf. goals:

* Not only outperform in small message sends, but excel with large ones. To be tested in three steps:
  * Step 1: wrap per-platform syscalls into a cross-platform foundation
  * Step 2: implement allocation & corking logic ontop
  * Step 3: build an example HTTP server and benchmark it with small sends, large sends & pipelined small sends.
  * Step 4: move µWebSockets ontop
