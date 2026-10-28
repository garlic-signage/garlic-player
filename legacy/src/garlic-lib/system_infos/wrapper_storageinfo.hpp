#ifndef WRAPPER_STORAGEINFO_HPP
#define WRAPPER_STORAGEINFO_HPP
#include "i_storageinfo.hpp"
class WrapperStorageInfo : public IStorageInfo
{
public:
    WrapperStorageInfo();
    ~WrapperStorageInfo() override = default;
    bool isValid() const override;
    bool isReady() const override;
    qint64 bytesAvailable() const override;
    qint64 bytesTotal() const override;
    void refresh() override;
    void setPath(const QString &path) override;
private:
    QStorageInfo MyStorageInfo;
    QString cache_path;
};
#endif // WRAPPER_STORAGEINFO_HPP
