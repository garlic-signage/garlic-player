#include "network_dialog.h"

NetworkDialog::NetworkDialog(QWidget *parent, TConfiguration *Config) : QDialog(parent), ui(new Ui::NetworkDialog)
{
    ui->setupUi(this);
    MyConfiguration = Config;
    connect(ui->cb_interface, SIGNAL(currentIndexChanged(QString )), this, SLOT(changeInterface(QString )));
    connect(ui->chk_dhcp, SIGNAL(stateChanged(int)), this, SLOT(changeDHCP(int)));

#ifdef SUPPORT_RPI
    script_directory =  MyConfiguration->getPaths("scripts")+"raspberry/";
#else
    script_directory =  MyConfiguration->getPaths("scripts")+"dummy/";
#endif

    scanInterfaces();

    bool is_dhcp = hasActiveDHCP();

    ui->chk_dhcp->setChecked(is_dhcp);
    toggleIPSection(!is_dhcp);
}

bool NetworkDialog::scanPossibleWiFiInterfaces(const QString interface)
{
    QProcess    process;
    QStringList arguments;
    bool        is_wifi =false;

    ui->cb_essid->clear();
    arguments << script_directory +"get_wifi.sh" << interface;
    process.start("/bin/sh", arguments);
    process.waitForFinished(-1); // will wait forever until finished

    QString stdout = process.readAllStandardOutput();
    QString stderr = process.readAllStandardError();
    if (stderr == "" && stdout != "")
    {
        QString wifi_net;
        foreach(wifi_net, stdout.split("\n"))
        {
            if (wifi_net != "")
                ui->cb_essid->addItem(wifi_net);
        }
        ui->cb_essid->setCurrentText(determineCurrentESSID());
        is_wifi = true;
    }
    return is_wifi;
}

bool NetworkDialog::commitToSystem()
{
    MyConfiguration->setNetworkInterface(ui->cb_interface->currentText());
    if (ui->cb_essid->isVisible()) // Is WiFi
    {
        if (ui->cb_essid->currentText() == "")
        {
            ui->lb_error->setText(tr("ESSID missed"));
            return false;
        }
        if (ui->edit_passphrase->text().length() < 8)
        {
            ui->lb_error->setText(tr("Passphrase must have at least 8 Letters"));
            return false;
        }
        writeWPASupplicantConf();
    }
    if (ui->chk_dhcp->checkState() == Qt::Unchecked)
    {
        QPair<QHostAddress, int> addr = QHostAddress::parseSubnet(ui->edit_ip->text()+"/"+ui->edit_netmask->text());
        if (addr.first.isNull())
        {
            ui->lb_error->setText(tr("IP is incorrect"));
            return false;
        }
        if (addr.second == -1)
        {
            ui->lb_error->setText(tr("Netmask is incorrect"));
            return false;
        }
        QHostAddress gateway(ui->edit_gateway->text());
        if (gateway.isNull())
        {
            ui->lb_error->setText(tr("Gateway is incorrect"));
            return false;
        }
        writeStaticIP();
    }
    else
        writeDhcp();

    return true;
}

void NetworkDialog::changeInterface(const QString &text)
{
    toggleWifiSection(scanPossibleWiFiInterfaces(text));
}

void NetworkDialog::changeDHCP(int state)
{
    if (state == Qt::Checked)
        toggleIPSection(false);
    else
        toggleIPSection(true);
}

void NetworkDialog::accept()
{
    ui->lb_error->setText("");
    if (commitToSystem())
        QDialog::accept();
}

bool NetworkDialog::hasActiveDHCP()
{
    QProcess process;
    QStringList arguments;

    QPair<QHostAddress, int> addr = QHostAddress::parseSubnet(ui->edit_ip->text()+"/"+ui->edit_netmask->text());
    arguments << script_directory+"/is_dhcp.sh";
    process.start("/bin/sh", arguments);
    process.waitForFinished(-1); // will wait forever until finished
    return (process.exitCode() ==0);
}

void NetworkDialog::toggleWifiSection(bool is_visible)
{
    ui->lb_essid->setVisible(is_visible);
    ui->cb_essid->setVisible(is_visible);
    ui->lb_encryption->setVisible(is_visible);
    ui->cb_encryption->setVisible(is_visible);
    ui->lb_passphrase->setVisible(is_visible);
    ui->edit_passphrase->setVisible(is_visible);
}

void NetworkDialog::toggleIPSection(bool is_visible)
{
    ui->lb_ip->setVisible(is_visible);
    ui->lb_netmask->setVisible(is_visible);
    ui->lb_gateway->setVisible(is_visible);
    ui->lb_dns->setVisible(is_visible);
    ui->edit_ip->setVisible(is_visible);
    ui->edit_netmask->setVisible(is_visible);
    ui->edit_gateway->setVisible(is_visible);
    ui->edit_dns->setVisible(is_visible);
    if (is_visible)
        determinceStaticIPs();
}

void NetworkDialog::writeWPASupplicantConf()
{
    QProcess process;
    QStringList arguments;

    arguments << MyConfiguration->getPaths("scripts")+"/write_wpa.sh" << ui->cb_essid->currentText() << ui->edit_passphrase->text();
    process.start("/bin/sh", arguments);

    process.waitForFinished(-1); // will wait forever until finished
}

void NetworkDialog::writeStaticIP()
{
    QProcess process;
    QStringList arguments;

    arguments << script_directory + "/write_static.sh"
              << ui->edit_ip->text()
              << ui->edit_netmask->text()
              << ui->edit_gateway->text()
              << ui->edit_dns->text();
    process.start("/bin/sh", arguments);

    process.waitForFinished(-1); // will wait forever until finished  
}

void NetworkDialog::writeDhcp()
{
    QProcess process;
    QStringList arguments;

    arguments << script_directory+"/write_dhcp.sh";
    process.start("/bin/sh", arguments);
    process.waitForFinished(-1); // will wait forever until finished
}

void NetworkDialog::determinceStaticIPs()
{
    QProcess process;
    QStringList arguments;

    arguments << script_directory + "get_static.sh";
    process.start("/bin/sh", arguments);

    process.waitForFinished(-1); // will wait forever until finished
    QString output = process.readAllStandardOutput();
    QStringList output_list = output.split(",");

    // remember output_list.at(1) is the interface
    if (output_list.size() > 1)
        ui->edit_ip->setText(output_list.at(1));
    if (output_list.size() > 2)
        ui->edit_netmask->setText(output_list.at(2));
    if (output_list.size() > 3)
        ui->edit_gateway->setText(output_list.at(3));
    if (output_list.size() > 4)
        ui->edit_dns->setText(output_list.at(4));
    return;

}

void NetworkDialog::scanInterfaces()
{
    QList<QNetworkInterface> allInterfaces = QNetworkInterface::allInterfaces();
    QNetworkInterface eth;
    QString current_interface = MyConfiguration->getNetworkInterface();
    foreach(eth, allInterfaces)
    {
        if (eth.flags().testFlag(QNetworkInterface::IsUp)
            && eth.flags().testFlag(QNetworkInterface::CanBroadcast)
            && eth.flags().testFlag(QNetworkInterface::CanMulticast)
        )
            ui->cb_interface->addItem(eth.name());

    }
    ui->cb_interface->setCurrentText(current_interface);
}

QString NetworkDialog::seperateValueFromParameter(const QString line)
{
    QStringList list = line.split("=");
    return list.at(1);
}

QString NetworkDialog::determineCurrentESSID()
{
    QProcess process;
    QStringList arguments;

    arguments << ui->cb_interface->currentText() << "--raw";
    process.start("iwgetid", arguments);

    process.waitForFinished(-1); // will wait forever until finished
    return process.readAllStandardOutput().trimmed();
}
