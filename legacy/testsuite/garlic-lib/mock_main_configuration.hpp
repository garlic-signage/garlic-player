#ifndef MOCK_MAIN_CONFIGURATION_HPP
#define MOCK_MAIN_CONFIGURATION_HPP

#include "i_main_configuration.hpp"
#include <gmock/gmock.h>

class MockMainConfiguration : public IMainConfiguration
{
public:
    MOCK_METHOD(void, init, (), (override));
    MOCK_METHOD(QString, getVersion, (), (override));
    MOCK_METHOD(void, setAdditionalVersion, (QString value), (override));
    MOCK_METHOD(void, setAppName, (QString value), (override));
    MOCK_METHOD(QString, getAppName, (), (override));
    MOCK_METHOD(QString, getDescription, (), (override));
    MOCK_METHOD(void, setLastPlayedIndexPath, (const QString &value), (override));
    MOCK_METHOD(QSettings*, getUserConfig, (), (override));
    MOCK_METHOD(QString, getUserConfigByKey, (QString key), (override));
    MOCK_METHOD(void, setUserConfigByKey, (QString key, QString value), (override));
    MOCK_METHOD(QString, createUuid, (), (override));
    MOCK_METHOD(void, setUuid, (const QString &value), (override));
    MOCK_METHOD(void, setPlayerName, (const QString &value), (override));
    MOCK_METHOD(void, determinePlayerName, (), (override));
    MOCK_METHOD(QString, determineApiAccessToken, (QString username, QString password), (override));
    MOCK_METHOD(QString, getApiAccessToken, (), (override));
    MOCK_METHOD(QString, getApiAccessTokenExpire, (), (override));
    MOCK_METHOD(QString, getUuid, (), (const, override));
    MOCK_METHOD(QString, getPlayerName, (), (const, override));
    MOCK_METHOD(void, setLogDir, (const QString &value), (override));
    MOCK_METHOD(void, setUserAgent, (const QString &value), (override));
    MOCK_METHOD(QString, getUserAgent, (), (const, override));
    MOCK_METHOD(QString, getIndexUri, (), (override));
    MOCK_METHOD(QString, getOS, (), (const, override));
    MOCK_METHOD(QString, getIndexPath, (), (override));
    MOCK_METHOD(QString, getTimeZone, (), (const, override));
    MOCK_METHOD(QString, getBasePath, (), (const, override));
    MOCK_METHOD(QString, getErrorText, (), (const, override));
    MOCK_METHOD(void, setValidatedContentUrl, (const QString &value), (override));
    MOCK_METHOD(QString, getValidatedContentUrl, (), (override));
    MOCK_METHOD(void, setStandbyMode, (const QString &value), (override));
    MOCK_METHOD(QString, getStandbyMode, (), (override));
    MOCK_METHOD(void, setRebootDays, (const QString &value), (override));
    MOCK_METHOD(QString, getRebootDays, (), (override));
    MOCK_METHOD(void, setRebootTime, (const QString &value), (override));
    MOCK_METHOD(QString, getRebootTime, (), (override));
    MOCK_METHOD(QString, getLastPlayedIndexPath, (), (override));
    MOCK_METHOD(QString, getStartTime, (), (const, override));
    MOCK_METHOD(void, setStartTime, (const QString &value), (override));
    MOCK_METHOD(QString, getPaths, (QString path_name), (override));
    MOCK_METHOD(void, setIndexUri, (const QString &value), (override));
    MOCK_METHOD(void, setIndexPath, (const QString &value), (override));
    MOCK_METHOD(void, setBasePath, (const QString &value), (override));
    MOCK_METHOD(void, determineBasePath, (QString absolute_path_to_bin), (override));
    MOCK_METHOD(void, determineIndexUri, (const QString &value), (override));
    MOCK_METHOD(void, createDirectories, (), (override));
    MOCK_METHOD(bool, validateContentUrl, (QString url_string), (override));
    MOCK_METHOD(void, determineUserAgent, (), (override));
};

#endif // MOCK_MAIN_CONFIGURATION_HPP
