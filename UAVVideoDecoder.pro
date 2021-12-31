QT += quick qml positioning
CONFIG += c++11

# The following define makes your compiler emit warnings if you use
# any feature of Qt which as been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

RESOURCES += qml.qrc

# Additional import path used to resolve QML modules in Qt Creator's code model
QML_IMPORT_PATH =

# Additional import path used to resolve QML modules just for Qt Quick Designer
QML_DESIGNER_IMPORT_PATH =

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

# GStreamer
unix:!macx: DEPENDPATH += /usr/local/include
unix:!macx: INCLUDEPATH += /usr/include/gstreamer-1.0
unix:!macx: INCLUDEPATH += /usr/lib/x86_64-linux-gnu/gstreamer-1.0/include
unix:!macx: INCLUDEPATH += /usr/include/glib-2.0
unix:!macx: INCLUDEPATH += /usr/lib/x86_64-linux-gnu/glib-2.0/include

unix:!macx: LIBS += -LD:\usr\lib\x86_64-linux-gnu\
    -lglib-2.0 \
    -lgstreamer-1.0 \
    -lgstapp-1.0 \
    -lgstrtsp-1.0 \
    -lgobject-2.0 \
    -lgstvideo-1.0
## FFMPEG
INCLUDEPATH += /usr/include/x86_64-linux-gnu/
DEPENDPATH += /usr/include/x86_64-linux-gnu/

LIBS +=  \
    -lavformat \
    -lavcodec \
    -lavutil \
    -lswscale \
    -lswresample

#//--------- lib zint
INCLUDEPATH += /usr/local/include
DEPENDPATH += /usr/local/include
LIBS += -L/usr/local/lib -lzint -lzbar

SOURCES += \
        main.cpp \
    Decoder/Gstreamer/GSTStreamDecoder.cpp \
    Decoder/DecoderInterface.cpp \
    VideoDisplay/I420Render.cpp \
    VideoDisplay/VideoRender.cpp \
    VideoEngine.cpp \
    Decoder/FFMpeg/FFMPEGDecoder.cpp \
    Decoder/MC/MCDecoder.cpp \
    Algorithm/AP_Math/Matrix3.cpp \
    Algorithm/AP_Math/Vector3.cpp \
    Algorithm/AP_TargetLocalization/TargetLocalization.cpp \
    Algorithm/Elevation.cpp

HEADERS += \
    Decoder/Gstreamer/GSTStreamDecoder.h \
    Decoder/DecoderInterface.h \
    VideoDisplay/I420Render.h \
    VideoDisplay/VideoRender.h \
    VideoEngine.h \
    Decoder/FFMpeg/FFMPEGDecoder.h \
    Decoder/MC/MCDecoder.h \
    Algorithm/AP_Math/AP_Math.h \
    Algorithm/AP_Math/Matrix3.h \
    Algorithm/AP_Math/Numbers.h \
    Algorithm/AP_Math/rotations.h \
    Algorithm/AP_Math/Vector3.h \
    Algorithm/AP_TargetLocalization/TargetLocalization.h \
    Algorithm/Elevation.h
