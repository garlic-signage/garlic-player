#include "wrapper_storageinfo.hpp"

#ifdef Q_OS_MACOS
#include <sys/statvfs.h>
#endif

WrapperStorageInfo::WrapperStorageInfo()
{
}

bool WrapperStorageInfo::isValid() const
{
#ifdef Q_OS_MACOS
    struct statvfs st;
    return statvfs(cache_path.toUtf8().constData(), &st) == 0;
#else
    return MyStorageInfo.isValid();
#endif
}

bool WrapperStorageInfo::isReady() const
{
#ifdef Q_OS_MACOS
    struct statvfs st;
    return statvfs(cache_path.toUtf8().constData(), &st) == 0;
#else
    return MyStorageInfo.isReady();
#endif
}

qint64 WrapperStorageInfo::bytesAvailable() const
{
#ifdef Q_OS_MACOS
    struct statvfs st;
    if (statvfs(cache_path.toUtf8().constData(), &st) == 0)
        return (qint64)st.f_bavail * st.f_frsize;
    return -1;
#else
    return MyStorageInfo.bytesAvailable();
#endif
}

qint64 WrapperStorageInfo::bytesTotal() const
{
#ifdef Q_OS_MACOS
    struct statvfs st;
    if (statvfs(cache_path.toUtf8().constData(), &st) == 0)
        return (qint64)st.f_blocks * st.f_frsize;
    return -1;
#else
    return MyStorageInfo.bytesTotal();
#endif
}

void WrapperStorageInfo::refresh()
{
#ifndef Q_OS_MACOS
    MyStorageInfo.refresh();
#endif
    // statvfs holt bei jedem Aufruf frische Werte, kein refresh nötig
}

void WrapperStorageInfo::setPath(const QString &path)
{
    cache_path = path;
#ifndef Q_OS_MACOS
    MyStorageInfo.setPath(path);
#endif
}
