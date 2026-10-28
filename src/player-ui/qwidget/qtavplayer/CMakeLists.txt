# QtAVPlayer of valbok (FFmpeg): in no distribution, so downloaded with a fixed tag and built as a static library.
# Its QtAVPlayer.cmake sets compile definitions for the whole directory, so it gets this directory of its own.
include(FetchContent)
# only the sources: the CMakeLists of QtAVPlayer builds a shared library with Qt Quick, so SOURCE_SUBDIR points to
# a directory without one and nothing of it is added
FetchContent_Declare(qtavplayer_source
    GIT_REPOSITORY https://github.com/valbok/QtAVPlayer.git
    GIT_TAG        v2026-10-04
    GIT_SHALLOW    TRUE
    SOURCE_SUBDIR  sources-only
)
FetchContent_MakeAvailable(qtavplayer_source)

# QtAVPlayer hands its frames to Qt Multimedia through private headers; that is known and wanted
set(QT_NO_PRIVATE_MODULE_WARNING ON)
find_package(Qt6 REQUIRED COMPONENTS Core Gui Multimedia MultimediaPrivate)

set(QT_AVPLAYER_DIR ${qtavplayer_source_SOURCE_DIR}/src/QtAVPlayer)
set(QT_AVPLAYER_MULTIMEDIA ON CACHE BOOL "" FORCE) # QAVAudioOutput and QVideoFrame for QVideoWidget
include(${QT_AVPLAYER_DIR}/QtAVPlayer.cmake)

add_library(qtavplayer STATIC ${QtAVPlayer_SOURCES})
set_target_properties(qtavplayer PROPERTIES AUTOMOC ON CXX_STANDARD 17)
target_compile_definitions(qtavplayer PUBLIC QT_AVPLAYER_MULTIMEDIA)
target_include_directories(qtavplayer SYSTEM PUBLIC ${qtavplayer_source_SOURCE_DIR}/src)
target_link_libraries(qtavplayer PUBLIC ${QtAVPlayer_LIBS} Qt6::Core Qt6::Gui Qt6::Multimedia Qt6::MultimediaPrivate)
