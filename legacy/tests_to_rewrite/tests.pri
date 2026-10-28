linux {
    LIBS += -L$$PWD/../src/lib -lgarlic
    LIBS += -L/usr/lib -lgmock -lgtest
    #temporary ToDO
    #  warning: template-id not allowed for constructor in C++20 in Arch gcc
    QMAKE_CXXFLAGS += -Wno-template-id-cdtor
}
macx {
    CONFIG+=sdk_no_version_check # remove annoying SDK messages on Apple Silicon
    QMAKE_APPLE_DEVICE_ARCHS = arm64 #x86_64 test only in arm64
    LIBS += -L$$PWD/../src/lib -lgarlic -lquazip -lzlib
    LIBS += -L/usr/lib -lgmock -lgtest

}
win32 {
    Release:LIBS += -L$$shadowed($$PWD)/lib -lgarlic -lquazip -lzlib
    Debug:LIBS += -L$$shadowed($$PWD)/lib -lgarlic -lquazipd -lzlib
}
