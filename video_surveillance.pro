QT       += core gui multimedia multimediawidgets

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    homepage.cpp \
    monitorpage.cpp \
    historypage.cpp \
    videopage.cpp \
    recordingthread.cpp \
    storagemanager.cpp \
    v4l2_wrapper.c

HEADERS += \
    mainwindow.h \
    homepage.h \
    monitorpage.h \
    historypage.h \
    videopage.h \
    recordingthread.h \
    storagemanager.h \
    v4l2_wrapper.h

FORMS += \
    mainwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

# 添加Linux下需要的库
unix {
    LIBS += -lv4l2
    
    # 添加FFmpeg相关库
    LIBS += -lavformat -lavcodec -lavutil -lswscale -lswresample
    
    # 可选：使用pkg-config获取编译和链接参数
    # CONFIG += link_pkgconfig
    # PKGCONFIG += libavformat libavcodec libavutil libswscale libswresample
}

RESOURCES += \
    res.qrc
