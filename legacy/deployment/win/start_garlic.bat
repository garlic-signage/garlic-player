set QT_PLUGIN_PATH=bin\plugins
set QT_QPA_PLATFORM_PLUGIN_PATH=bin\platforms

set QT_LOGGING_RULES="*.debug=false"

bin\garlic-player.exe %1 %2
