#include "cmdparser.h"

TCmdParser::TCmdParser(MainConfiguration *config)
{
    MyMainConfiguration = config;
    parser.setApplicationDescription(MyMainConfiguration->getDescription());
}

void TCmdParser::addOptions()
{
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOptions(
    {
        {{"c", "config"}, "path to a config.xml", "config"},
        {{"m", "windows-mode"}, "fullscreen, bigscreen (when multimonitor) or windowed mode", "fullscreen|bigscreen|windowed"},
        {{"z", "windows-size"}, "size of windows (when windows mode) e.g. 980x540 means 980px width and 540px height", "width x height"},
        {{"s", "screen-select"}, "Starts in selected screen", "screen" }
                    ,
        {{"d", "disable-web-security"}, "", "" },
        {{"u", "user-data-dir"}, "path to less secured", "path" }
    });
    parser.addPositionalArgument("SMIL_INDEX", QCoreApplication::translate("main", "Path to SMIL index"));
    parser.setOptionsAfterPositionalArgumentsMode(QCommandLineParser::ParseAsOptions);
}

bool TCmdParser::parse(LibFacade *MyLibFacade)
{
    parser.process(qApp->arguments());
    const QStringList args = parser.positionalArguments();

    if (parser.isSet("s"))
        setScreenSelect(parser.value("s").toInt());

    if (parser.isSet("m"))
    {
        QString val = parser.value("m");
        if (val == "fullscreen" || val == "bigscreen" || val == "windowed")
            setWindowMode(val);
    }

    if (parser.isSet("m"))
    {
        QString val = parser.value("m");
        if (val == "fullscreen" || val == "bigscreen" || val == "windowed")
            setWindowMode(val);
    }

    if (parser.isSet("z"))
    {
        QStringList list = parser.value("z").split("x");
        if (list.length() == 2)
        {
            int x = list.at(0).toInt();
            int y = list.at(1).toInt();
            if (x > 0 && y > 0)
                setWindowSize(QSize(x, y));
        }
    }

    if (parser.isSet("c"))
    {
        QFileInfo file(parser.value("c"));
        if (file.exists())
        {
            MyLibFacade->setConfigFromExternal(file.absoluteFilePath(), false);
        }
    }

    if (args.size() > 0)
    {
        if (MyMainConfiguration->validateContentUrl(args.at(0)))
           MyMainConfiguration->determineIndexUri(MyMainConfiguration->getValidatedContentUrl());
        else
        {
            QTextStream ts( stdout );
            ts << "\n" << MyMainConfiguration->getErrorText() << "\n";
            return false;
        }
    }
    else
        MyMainConfiguration->determineIndexUri("");

    return true;
}

QString TCmdParser::getWindowMode() const
{
    return window_mode;
}

void TCmdParser::setWindowMode(const QString &value)
{
    window_mode = value;
}

QSize TCmdParser::getWindowSize()
{
    return window_size;
}

void TCmdParser::setWindowSize(QSize value)
{
    window_size = value;
}

int TCmdParser::getScreenSelect()
{
    return screen_select;
}

void TCmdParser::setScreenSelect(int value)
{
    screen_select = value;
}

QString TCmdParser::getIndexUrl() const
{
    return index_url;
}

void TCmdParser::setIndexUrl(const QString &value)
{
    index_url = value;
}
