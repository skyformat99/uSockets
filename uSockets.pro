TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += main.cpp \
    Context.cpp \
    Socket.cpp \
    Berkeley.cpp

HEADERS += \
    Context.h \
    Socket.h \
    Berkeley.h
