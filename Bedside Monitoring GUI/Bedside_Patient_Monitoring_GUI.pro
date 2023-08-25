QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    network_topology.cpp \
    uart.cpp

HEADERS += \
    custom_struct.h \
    mainwindow.h \
    network_topology.h \
    uart.h

FORMS += \
    mainwindow.ui \
    network_topology.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

#-------------------------------------------------
# This section will include QextSerialPort in
# your project:

HOMEDIR = $$(HOME)
include($$HOMEDIR/qextserialport/src/qextserialport.pri)

## Include Contiki paths

#INCLUDEPATH += $$HOMEDIR/contiki/core
#INCLUDEPATH += $$HOMEDIR/contiki/platform
#INCLUDEPATH += $$HOMEDIR/contiki/platform/zoul
#INCLUDEPATH += $$HOMEDIR/contiki/platform/zoul/orion
#INCLUDEPATH += $$HOMEDIR/contiki/cpu/cc2538/dev

#LIBS += -L/$$HOMEDIR/contiki/lib -lcontiki
