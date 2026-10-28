#ifndef I_MAIN_CONFIGURATION_HPP
#define I_MAIN_CONFIGURATION_HPP

#include <QSettings>
#include <QString>

class IMainConfiguration
{
    public:
        virtual ~IMainConfiguration() = default;

        virtual void init() = 0;
        virtual QString getVersion() = 0;
        virtual void setAdditionalVersion(QString value) = 0;
        virtual void setAppName(QString value) = 0;
        virtual QString getAppName() = 0;
        virtual QString getDescription() = 0;

        virtual void setLastPlayedIndexPath(const QString &value) = 0;
        virtual QSettings* getUserConfig() = 0;
        virtual QString getUserConfigByKey(QString key) = 0;
        virtual void setUserConfigByKey(QString key, QString value) = 0;
        virtual QString createUuid() = 0;
        virtual void setUuid(const QString &value) = 0;
        virtual void setPlayerName(const QString &value) = 0;
        virtual void determinePlayerName() = 0;
        virtual QString determineApiAccessToken(QString username, QString password) = 0;
        virtual QString getApiAccessToken() = 0;
        virtual QString getApiAccessTokenExpire() = 0;
        virtual QString getUuid() const = 0;
        virtual QString getPlayerName() const = 0;
        virtual void setLogDir(const QString &value) = 0;
        virtual void setUserAgent(const QString &value) = 0;
        virtual QString getUserAgent() const = 0;
        virtual QString getIndexUri() = 0;
        virtual QString getOS() const = 0;
        virtual QString getIndexPath() = 0;
        virtual QString getTimeZone() const = 0;
        virtual QString getBasePath() const = 0;
        virtual QString getErrorText() const = 0;
        virtual void setValidatedContentUrl(const QString &value) = 0;
        virtual QString getValidatedContentUrl() = 0;
        virtual void setStandbyMode(const QString &value) = 0;
        virtual QString getStandbyMode() = 0;
        virtual void setRebootDays(const QString &value) = 0;
        virtual QString getRebootDays() = 0;
        virtual void setRebootTime(const QString &value) = 0;
        virtual QString getRebootTime() = 0;
        virtual QString getLastPlayedIndexPath() = 0;
        virtual QString getStartTime() const = 0;
        virtual void setStartTime(const QString &value) = 0;
        virtual QString getPaths(QString path_name) = 0;
        virtual void setIndexUri(const QString &value) = 0;
        virtual void setIndexPath(const QString &value) = 0;
//        virtual void setNetworkInterface(const QString &value) = 0;
//        virtual QString getNetworkInterface() = 0;
        virtual void setBasePath(const QString &value) = 0;
        virtual void determineBasePath(QString absolute_path_to_bin) = 0;
        virtual void determineIndexUri(const QString &value) = 0;
        virtual void createDirectories() = 0;
        virtual bool validateContentUrl(QString url_string) = 0;
        virtual void determineUserAgent() = 0;

        // Static members are not part of the interface, as they cannot be overridden.
        static QString log_directory;
        static QString getLogDir();
};

#endif // I_MAIN_CONFIGURATION_HPP
