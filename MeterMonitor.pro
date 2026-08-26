#-------------------------------------------------
# MeterMonitor.pro
# Qt 6.6.2 / MSVC2019 64bit
#
# 빌드 방법)
#   1) Qt Creator 에서 이 .pro 파일을 열고
#      Kit = "Desktop Qt 6.6.2 MSVC2019 64bit" 선택 후 빌드
#   2) 또는 "x64 Native Tools Command Prompt for VS 2019" 에서
#        C:\Qt\6.6.2\msvc2019_64\bin\qmake.exe MeterMonitor.pro
#        nmake            (또는 jom)
#-------------------------------------------------

QT       += core gui widgets serialport

CONFIG   += c++17
CONFIG   -= console

TARGET   = MeterMonitor
TEMPLATE = app

# 소스가 UTF-8 이므로 MSVC 에서 한글이 깨지지 않도록 지정
win32-msvc* {
    QMAKE_CXXFLAGS += /utf-8
    QMAKE_CFLAGS   += /utf-8
}

DEFINES += QT_DEPRECATED_WARNINGS

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    serialsettingsdialog.cpp \
    parsingsettingsdialog.cpp \
    graphwidget.cpp

HEADERS += \
    mainwindow.h \
    serialsettingsdialog.h \
    parsingsettingsdialog.h \
    graphwidget.h \
    unitutil.h

# 기본 배포 규칙
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
