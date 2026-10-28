#include "configdialog.h"

ConfigDialog::ConfigDialog(QWidget *parent, MainConfiguration *Config) :  QDialog(parent), ui(new Ui::ConfigDialog)
{
    ui->setupUi(this);
    MyConfiguration = Config;
    ui->lineEditPlayerName->setText(MyConfiguration->getPlayerName());
    ui->lineEditContentUrl->setText(MyConfiguration->getIndexUri());
#if !defined  Q_OS_ANDROID
    // cause in Android it shows fullscreen and not as dialog
    setWindowFlags(Qt::WindowStaysOnTopHint);
#endif

}

ConfigDialog::~ConfigDialog()
{
    delete ui;
}

// virtual override
void ConfigDialog::showEvent( QShowEvent* showEvent )
{
    QDialog::showEvent( showEvent);
    activateWindow();
}

void ConfigDialog::accept()
{
    if (MyConfiguration->validateContentUrl(ui->lineEditContentUrl->text()))
    {
        MyConfiguration->setPlayerName(ui->lineEditPlayerName->text());
        MyConfiguration->determineIndexUri(MyConfiguration->getValidatedContentUrl());
        MyConfiguration->determineUserAgent();
        QDialog::accept();
    }
    else
        ui->labelErrorMessage->setText(MyConfiguration->getErrorText());
}


