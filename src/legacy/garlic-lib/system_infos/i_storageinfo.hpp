#ifndef I_STORAGEINFO_HPP
#define I_STORAGEINFO_HPP

#include <QStorageInfo>

class IStorageInfo
{
    public:
        virtual ~IStorageInfo() {}
        virtual bool isValid() const = 0;
        virtual bool isReady() const = 0;
        virtual qint64 bytesAvailable() const = 0;
        virtual qint64 bytesTotal() const = 0;
        virtual void refresh() = 0;
        virtual void setPath(const QString & ) = 0;
};

#endif // I_STORAGEINFO_HPP
