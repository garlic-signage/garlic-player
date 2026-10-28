#ifndef NETWORK_DIALOG_H
#define NETWORK_DIALOG_H

#include <QObject>
#include <QDialog>
#include <QDir>
#include <QNetworkInterface>
#include <QProcess>
#include <QHostAddress>

#include <QDebug>

#include "tools/configuration.h"

namespace Ui
{
    class NetworkDialog;
}
#ifdef SUPPORT_EMBEDDED
    #include "ui_networkdialog.h"
#endif

class NetworkDialog : public QDialog
{
        Q_OBJECT
    public:
        explicit NetworkDialog(QWidget *parent, TConfiguration *Config);

    protected:
        Ui::NetworkDialog *ui;
        TConfiguration    *MyConfiguration = NULL;
        bool scanPossibleWiFiInterfaces(const QString interface);
        bool commitToSystem();
    protected slots:
        void changeInterface(const QString &text);
        void changeDHCP(int state);
        void accept();
    private:
        QString script_directory;
        bool hasActiveDHCP();
        void toggleWifiSection(bool is_visible);
        void toggleIPSection(bool is_visible);
        void writeWPASupplicantConf();
        void writeStaticIP();
        void writeDhcp();
        void determinceStaticIPs();
        void scanInterfaces();
        QString seperateValueFromParameter(const QString line);
        QString determineCurrentESSID();
    signals:

};

#endif // NETWORK_DIALOG_H
