#ifndef DISKSPACE_HPP
#define DISKSPACE_HPP

#include "i_storageinfo.hpp"

namespace SystemInfos
{
    class DiscSpace : public QObject
    {
            Q_OBJECT
        public:
            DiscSpace(IStorageInfo *si, QObject *parent = nullptr);
            void     init(QString path);
            void     recalculate();
            quint64 getBytesTotal();
            quint64 getBytesFree();
            quint64 getBytesLocked();
        private:
            const float    _max_fill    = 0.96F; // let 4% of total disc space free for Operating System logs, tmp etc
            quint64        calculateFreeBytes();
            quint64        bytes_locked = 0;
            quint64        bytes_total  = 0;
            quint64        bytes_free   = 0;
            QString        cache_path   = "";
            IStorageInfo  *MyStorage;

    };
}
#endif // DISKSPACE_HPP
