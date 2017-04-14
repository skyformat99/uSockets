TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += main.cpp \
    Berkeley.cpp \
    Epoll.cpp

HEADERS += \
    Berkeley.h \
    Epoll.h
