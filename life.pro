

QT       += core gui sql network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

TARGET = LifeManager
TEMPLATE = app

# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated.
DEFINES += QT_DEPRECATED_WARNINGS

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    leftpanel.cpp \
    centerpanel.cpp \
    rightpanel.cpp \
    datamanager.cpp \
    scheduledialog.cpp \
    taskdialog.cpp \
    dietdialog.cpp

HEADERS += \
    mainwindow.h \
    leftpanel.h \
    centerpanel.h \
    rightpanel.h \
    datamanager.h \
    scheduledialog.h \
    taskdialog.h \
    dietdialog.h

FORMS += \
    mainwindow.ui \
    leftpanel.ui \
    centerpanel.ui \
    rightpanel.ui

RESOURCES += \
    resources.qrc

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

# Windows specific
win32 {
    RC_ICONS = resources/style/cat.ico
}
