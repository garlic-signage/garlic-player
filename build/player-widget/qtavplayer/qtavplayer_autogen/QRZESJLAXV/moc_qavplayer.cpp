/****************************************************************************
** Meta object code from reading C++ file 'qavplayer.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../_deps/qtavplayer_source-src/src/QtAVPlayer/qavplayer.h"
#include <QtCore/qmetatype.h>
#include <QtCore/QList>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'qavplayer.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 69
#error "This file was generated using the moc from 6.11.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
QT_WARNING_DISABLE_GCC("-Wuseless-cast")
namespace {
struct qt_meta_tag_ZN9QAVPlayerE_t {};
} // unnamed namespace

template <> constexpr inline auto QAVPlayer::qt_create_metaobjectdata<qt_meta_tag_ZN9QAVPlayerE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "QAVPlayer",
        "sourceChanged",
        "",
        "url",
        "outputChanged",
        "filename",
        "stateChanged",
        "QAVPlayer::State",
        "newState",
        "mediaStatusChanged",
        "QAVPlayer::MediaStatus",
        "status",
        "errorOccurred",
        "QAVPlayer::Error",
        "str",
        "durationChanged",
        "duration",
        "seekableChanged",
        "seekable",
        "speedChanged",
        "rate",
        "videoFrameRateChanged",
        "videoStreamsChanged",
        "QList<QAVStream>",
        "streams",
        "audioStreamsChanged",
        "subtitleStreamsChanged",
        "played",
        "pos",
        "paused",
        "stopped",
        "stepped",
        "seeked",
        "filtersChanged",
        "filters",
        "bitstreamFilterChanged",
        "desc",
        "syncedChanged",
        "sync",
        "inputFormatChanged",
        "format",
        "inputVideoCodecChanged",
        "codec",
        "inputOptionsChanged",
        "QMap<QString,QString>",
        "opts",
        "videoCodecOptionsChanged",
        "videoFrame",
        "QAVVideoFrame",
        "frame",
        "audioFrame",
        "QAVAudioFrame",
        "subtitleFrame",
        "QAVSubtitleFrame",
        "play",
        "pause",
        "stop",
        "seek",
        "position",
        "setSpeed",
        "stepForward",
        "stepBackward",
        "State",
        "StoppedState",
        "PlayingState",
        "PausedState",
        "MediaStatus",
        "NoMedia",
        "LoadedMedia",
        "EndOfMedia",
        "InvalidMedia",
        "Error",
        "NoError",
        "ResourceError",
        "FilterError",
        "BitstreamFilterError",
        "MuxerError"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'sourceChanged'
        QtMocHelpers::SignalData<void(const QString &)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 3 },
        }}),
        // Signal 'outputChanged'
        QtMocHelpers::SignalData<void(const QString &)>(4, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 5 },
        }}),
        // Signal 'stateChanged'
        QtMocHelpers::SignalData<void(QAVPlayer::State)>(6, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 7, 8 },
        }}),
        // Signal 'mediaStatusChanged'
        QtMocHelpers::SignalData<void(QAVPlayer::MediaStatus)>(9, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 10, 11 },
        }}),
        // Signal 'errorOccurred'
        QtMocHelpers::SignalData<void(QAVPlayer::Error, const QString &)>(12, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 13, 2 }, { QMetaType::QString, 14 },
        }}),
        // Signal 'durationChanged'
        QtMocHelpers::SignalData<void(qint64)>(15, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::LongLong, 16 },
        }}),
        // Signal 'seekableChanged'
        QtMocHelpers::SignalData<void(bool)>(17, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 18 },
        }}),
        // Signal 'speedChanged'
        QtMocHelpers::SignalData<void(qreal)>(19, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QReal, 20 },
        }}),
        // Signal 'videoFrameRateChanged'
        QtMocHelpers::SignalData<void(double)>(21, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Double, 20 },
        }}),
        // Signal 'videoStreamsChanged'
        QtMocHelpers::SignalData<void(const QList<QAVStream> &)>(22, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 23, 24 },
        }}),
        // Signal 'audioStreamsChanged'
        QtMocHelpers::SignalData<void(const QList<QAVStream> &)>(25, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 23, 24 },
        }}),
        // Signal 'subtitleStreamsChanged'
        QtMocHelpers::SignalData<void(const QList<QAVStream> &)>(26, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 23, 24 },
        }}),
        // Signal 'played'
        QtMocHelpers::SignalData<void(qint64)>(27, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::LongLong, 28 },
        }}),
        // Signal 'paused'
        QtMocHelpers::SignalData<void(qint64)>(29, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::LongLong, 28 },
        }}),
        // Signal 'stopped'
        QtMocHelpers::SignalData<void(qint64)>(30, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::LongLong, 28 },
        }}),
        // Signal 'stepped'
        QtMocHelpers::SignalData<void(qint64)>(31, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::LongLong, 28 },
        }}),
        // Signal 'seeked'
        QtMocHelpers::SignalData<void(qint64)>(32, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::LongLong, 28 },
        }}),
        // Signal 'filtersChanged'
        QtMocHelpers::SignalData<void(const QList<QString> &)>(33, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QStringList, 34 },
        }}),
        // Signal 'bitstreamFilterChanged'
        QtMocHelpers::SignalData<void(const QString &)>(35, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 36 },
        }}),
        // Signal 'syncedChanged'
        QtMocHelpers::SignalData<void(bool)>(37, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 38 },
        }}),
        // Signal 'inputFormatChanged'
        QtMocHelpers::SignalData<void(const QString &)>(39, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 40 },
        }}),
        // Signal 'inputVideoCodecChanged'
        QtMocHelpers::SignalData<void(const QString &)>(41, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 42 },
        }}),
        // Signal 'inputOptionsChanged'
        QtMocHelpers::SignalData<void(const QMap<QString,QString> &)>(43, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 44, 45 },
        }}),
        // Signal 'videoCodecOptionsChanged'
        QtMocHelpers::SignalData<void(const QMap<QString,QString> &)>(46, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 44, 45 },
        }}),
        // Signal 'videoFrame'
        QtMocHelpers::SignalData<void(const QAVVideoFrame &)>(47, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 48, 49 },
        }}),
        // Signal 'audioFrame'
        QtMocHelpers::SignalData<void(const QAVAudioFrame &)>(50, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 51, 49 },
        }}),
        // Signal 'subtitleFrame'
        QtMocHelpers::SignalData<void(const QAVSubtitleFrame &)>(52, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 53, 49 },
        }}),
        // Slot 'play'
        QtMocHelpers::SlotData<void()>(54, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'pause'
        QtMocHelpers::SlotData<void()>(55, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'stop'
        QtMocHelpers::SlotData<void()>(56, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'seek'
        QtMocHelpers::SlotData<void(qint64)>(57, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::LongLong, 58 },
        }}),
        // Slot 'setSpeed'
        QtMocHelpers::SlotData<void(qreal)>(59, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QReal, 20 },
        }}),
        // Slot 'stepForward'
        QtMocHelpers::SlotData<void()>(60, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'stepBackward'
        QtMocHelpers::SlotData<void()>(61, 2, QMC::AccessPublic, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
        // enum 'State'
        QtMocHelpers::EnumData<enum State>(62, 62, QMC::EnumFlags{}).add({
            {   63, State::StoppedState },
            {   64, State::PlayingState },
            {   65, State::PausedState },
        }),
        // enum 'MediaStatus'
        QtMocHelpers::EnumData<enum MediaStatus>(66, 66, QMC::EnumFlags{}).add({
            {   67, MediaStatus::NoMedia },
            {   68, MediaStatus::LoadedMedia },
            {   69, MediaStatus::EndOfMedia },
            {   70, MediaStatus::InvalidMedia },
        }),
        // enum 'Error'
        QtMocHelpers::EnumData<enum Error>(71, 71, QMC::EnumFlags{}).add({
            {   72, Error::NoError },
            {   73, Error::ResourceError },
            {   74, Error::FilterError },
            {   75, Error::BitstreamFilterError },
            {   76, Error::MuxerError },
        }),
    };
    return QtMocHelpers::metaObjectData<QAVPlayer, qt_meta_tag_ZN9QAVPlayerE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject QAVPlayer::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN9QAVPlayerE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN9QAVPlayerE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN9QAVPlayerE_t>.metaTypes,
    nullptr
} };

void QAVPlayer::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<QAVPlayer *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->sourceChanged((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 1: _t->outputChanged((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 2: _t->stateChanged((*reinterpret_cast<std::add_pointer_t<QAVPlayer::State>>(_a[1]))); break;
        case 3: _t->mediaStatusChanged((*reinterpret_cast<std::add_pointer_t<QAVPlayer::MediaStatus>>(_a[1]))); break;
        case 4: _t->errorOccurred((*reinterpret_cast<std::add_pointer_t<QAVPlayer::Error>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 5: _t->durationChanged((*reinterpret_cast<std::add_pointer_t<qint64>>(_a[1]))); break;
        case 6: _t->seekableChanged((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 7: _t->speedChanged((*reinterpret_cast<std::add_pointer_t<qreal>>(_a[1]))); break;
        case 8: _t->videoFrameRateChanged((*reinterpret_cast<std::add_pointer_t<double>>(_a[1]))); break;
        case 9: _t->videoStreamsChanged((*reinterpret_cast<std::add_pointer_t<QList<QAVStream>>>(_a[1]))); break;
        case 10: _t->audioStreamsChanged((*reinterpret_cast<std::add_pointer_t<QList<QAVStream>>>(_a[1]))); break;
        case 11: _t->subtitleStreamsChanged((*reinterpret_cast<std::add_pointer_t<QList<QAVStream>>>(_a[1]))); break;
        case 12: _t->played((*reinterpret_cast<std::add_pointer_t<qint64>>(_a[1]))); break;
        case 13: _t->paused((*reinterpret_cast<std::add_pointer_t<qint64>>(_a[1]))); break;
        case 14: _t->stopped((*reinterpret_cast<std::add_pointer_t<qint64>>(_a[1]))); break;
        case 15: _t->stepped((*reinterpret_cast<std::add_pointer_t<qint64>>(_a[1]))); break;
        case 16: _t->seeked((*reinterpret_cast<std::add_pointer_t<qint64>>(_a[1]))); break;
        case 17: _t->filtersChanged((*reinterpret_cast<std::add_pointer_t<QList<QString>>>(_a[1]))); break;
        case 18: _t->bitstreamFilterChanged((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 19: _t->syncedChanged((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 20: _t->inputFormatChanged((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 21: _t->inputVideoCodecChanged((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 22: _t->inputOptionsChanged((*reinterpret_cast<std::add_pointer_t<QMap<QString,QString>>>(_a[1]))); break;
        case 23: _t->videoCodecOptionsChanged((*reinterpret_cast<std::add_pointer_t<QMap<QString,QString>>>(_a[1]))); break;
        case 24: _t->videoFrame((*reinterpret_cast<std::add_pointer_t<QAVVideoFrame>>(_a[1]))); break;
        case 25: _t->audioFrame((*reinterpret_cast<std::add_pointer_t<QAVAudioFrame>>(_a[1]))); break;
        case 26: _t->subtitleFrame((*reinterpret_cast<std::add_pointer_t<QAVSubtitleFrame>>(_a[1]))); break;
        case 27: _t->play(); break;
        case 28: _t->pause(); break;
        case 29: _t->stop(); break;
        case 30: _t->seek((*reinterpret_cast<std::add_pointer_t<qint64>>(_a[1]))); break;
        case 31: _t->setSpeed((*reinterpret_cast<std::add_pointer_t<qreal>>(_a[1]))); break;
        case 32: _t->stepForward(); break;
        case 33: _t->stepBackward(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
        case 2:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< QAVPlayer::State >(); break;
            }
            break;
        case 3:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< QAVPlayer::MediaStatus >(); break;
            }
            break;
        case 4:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< QAVPlayer::Error >(); break;
            }
            break;
        case 9:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< QList<QAVStream> >(); break;
            }
            break;
        case 10:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< QList<QAVStream> >(); break;
            }
            break;
        case 11:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< QList<QAVStream> >(); break;
            }
            break;
        case 24:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< QAVVideoFrame >(); break;
            }
            break;
        case 25:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< QAVAudioFrame >(); break;
            }
            break;
        case 26:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< QAVSubtitleFrame >(); break;
            }
            break;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (QAVPlayer::*)(const QString & )>(_a, &QAVPlayer::sourceChanged, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (QAVPlayer::*)(const QString & )>(_a, &QAVPlayer::outputChanged, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (QAVPlayer::*)(QAVPlayer::State )>(_a, &QAVPlayer::stateChanged, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (QAVPlayer::*)(QAVPlayer::MediaStatus )>(_a, &QAVPlayer::mediaStatusChanged, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (QAVPlayer::*)(QAVPlayer::Error , const QString & )>(_a, &QAVPlayer::errorOccurred, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (QAVPlayer::*)(qint64 )>(_a, &QAVPlayer::durationChanged, 5))
            return;
        if (QtMocHelpers::indexOfMethod<void (QAVPlayer::*)(bool )>(_a, &QAVPlayer::seekableChanged, 6))
            return;
        if (QtMocHelpers::indexOfMethod<void (QAVPlayer::*)(qreal )>(_a, &QAVPlayer::speedChanged, 7))
            return;
        if (QtMocHelpers::indexOfMethod<void (QAVPlayer::*)(double )>(_a, &QAVPlayer::videoFrameRateChanged, 8))
            return;
        if (QtMocHelpers::indexOfMethod<void (QAVPlayer::*)(const QList<QAVStream> & )>(_a, &QAVPlayer::videoStreamsChanged, 9))
            return;
        if (QtMocHelpers::indexOfMethod<void (QAVPlayer::*)(const QList<QAVStream> & )>(_a, &QAVPlayer::audioStreamsChanged, 10))
            return;
        if (QtMocHelpers::indexOfMethod<void (QAVPlayer::*)(const QList<QAVStream> & )>(_a, &QAVPlayer::subtitleStreamsChanged, 11))
            return;
        if (QtMocHelpers::indexOfMethod<void (QAVPlayer::*)(qint64 )>(_a, &QAVPlayer::played, 12))
            return;
        if (QtMocHelpers::indexOfMethod<void (QAVPlayer::*)(qint64 )>(_a, &QAVPlayer::paused, 13))
            return;
        if (QtMocHelpers::indexOfMethod<void (QAVPlayer::*)(qint64 )>(_a, &QAVPlayer::stopped, 14))
            return;
        if (QtMocHelpers::indexOfMethod<void (QAVPlayer::*)(qint64 )>(_a, &QAVPlayer::stepped, 15))
            return;
        if (QtMocHelpers::indexOfMethod<void (QAVPlayer::*)(qint64 )>(_a, &QAVPlayer::seeked, 16))
            return;
        if (QtMocHelpers::indexOfMethod<void (QAVPlayer::*)(const QList<QString> & )>(_a, &QAVPlayer::filtersChanged, 17))
            return;
        if (QtMocHelpers::indexOfMethod<void (QAVPlayer::*)(const QString & )>(_a, &QAVPlayer::bitstreamFilterChanged, 18))
            return;
        if (QtMocHelpers::indexOfMethod<void (QAVPlayer::*)(bool )>(_a, &QAVPlayer::syncedChanged, 19))
            return;
        if (QtMocHelpers::indexOfMethod<void (QAVPlayer::*)(const QString & )>(_a, &QAVPlayer::inputFormatChanged, 20))
            return;
        if (QtMocHelpers::indexOfMethod<void (QAVPlayer::*)(const QString & )>(_a, &QAVPlayer::inputVideoCodecChanged, 21))
            return;
        if (QtMocHelpers::indexOfMethod<void (QAVPlayer::*)(const QMap<QString,QString> & )>(_a, &QAVPlayer::inputOptionsChanged, 22))
            return;
        if (QtMocHelpers::indexOfMethod<void (QAVPlayer::*)(const QMap<QString,QString> & )>(_a, &QAVPlayer::videoCodecOptionsChanged, 23))
            return;
        if (QtMocHelpers::indexOfMethod<void (QAVPlayer::*)(const QAVVideoFrame & )>(_a, &QAVPlayer::videoFrame, 24))
            return;
        if (QtMocHelpers::indexOfMethod<void (QAVPlayer::*)(const QAVAudioFrame & )>(_a, &QAVPlayer::audioFrame, 25))
            return;
        if (QtMocHelpers::indexOfMethod<void (QAVPlayer::*)(const QAVSubtitleFrame & )>(_a, &QAVPlayer::subtitleFrame, 26))
            return;
    }
}

const QMetaObject *QAVPlayer::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *QAVPlayer::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN9QAVPlayerE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int QAVPlayer::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 34)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 34;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 34)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 34;
    }
    return _id;
}

// SIGNAL 0
void QAVPlayer::sourceChanged(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}

// SIGNAL 1
void QAVPlayer::outputChanged(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1);
}

// SIGNAL 2
void QAVPlayer::stateChanged(QAVPlayer::State _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1);
}

// SIGNAL 3
void QAVPlayer::mediaStatusChanged(QAVPlayer::MediaStatus _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1);
}

// SIGNAL 4
void QAVPlayer::errorOccurred(QAVPlayer::Error _t1, const QString & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 4, nullptr, _t1, _t2);
}

// SIGNAL 5
void QAVPlayer::durationChanged(qint64 _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 5, nullptr, _t1);
}

// SIGNAL 6
void QAVPlayer::seekableChanged(bool _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 6, nullptr, _t1);
}

// SIGNAL 7
void QAVPlayer::speedChanged(qreal _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 7, nullptr, _t1);
}

// SIGNAL 8
void QAVPlayer::videoFrameRateChanged(double _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 8, nullptr, _t1);
}

// SIGNAL 9
void QAVPlayer::videoStreamsChanged(const QList<QAVStream> & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 9, nullptr, _t1);
}

// SIGNAL 10
void QAVPlayer::audioStreamsChanged(const QList<QAVStream> & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 10, nullptr, _t1);
}

// SIGNAL 11
void QAVPlayer::subtitleStreamsChanged(const QList<QAVStream> & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 11, nullptr, _t1);
}

// SIGNAL 12
void QAVPlayer::played(qint64 _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 12, nullptr, _t1);
}

// SIGNAL 13
void QAVPlayer::paused(qint64 _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 13, nullptr, _t1);
}

// SIGNAL 14
void QAVPlayer::stopped(qint64 _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 14, nullptr, _t1);
}

// SIGNAL 15
void QAVPlayer::stepped(qint64 _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 15, nullptr, _t1);
}

// SIGNAL 16
void QAVPlayer::seeked(qint64 _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 16, nullptr, _t1);
}

// SIGNAL 17
void QAVPlayer::filtersChanged(const QList<QString> & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 17, nullptr, _t1);
}

// SIGNAL 18
void QAVPlayer::bitstreamFilterChanged(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 18, nullptr, _t1);
}

// SIGNAL 19
void QAVPlayer::syncedChanged(bool _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 19, nullptr, _t1);
}

// SIGNAL 20
void QAVPlayer::inputFormatChanged(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 20, nullptr, _t1);
}

// SIGNAL 21
void QAVPlayer::inputVideoCodecChanged(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 21, nullptr, _t1);
}

// SIGNAL 22
void QAVPlayer::inputOptionsChanged(const QMap<QString,QString> & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 22, nullptr, _t1);
}

// SIGNAL 23
void QAVPlayer::videoCodecOptionsChanged(const QMap<QString,QString> & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 23, nullptr, _t1);
}

// SIGNAL 24
void QAVPlayer::videoFrame(const QAVVideoFrame & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 24, nullptr, _t1);
}

// SIGNAL 25
void QAVPlayer::audioFrame(const QAVAudioFrame & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 25, nullptr, _t1);
}

// SIGNAL 26
void QAVPlayer::subtitleFrame(const QAVSubtitleFrame & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 26, nullptr, _t1);
}
QT_WARNING_POP
