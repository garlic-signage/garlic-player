# directory src

Garlic-player is splitted into garlic-lib and different player-xxx. The so called player views.

- better unit testing for the library
- more platform indepence
- independence of the "view"-component 

The player-view is a small part which is responsible for the visualisation and UI. 

For example, due to the independence it is possible to replace the Android version with a native Java implementation.

As Qt-Widget are not full compatible with Android or iOS we need so or so two different views.

Unfortunately I faced a lot of bugs and problems with QML. So it makes no sense to stuck on a QML Version from my point of view. 

## garlic-lib

The basic lib for the player without dependencies to the view component and for testing.
It integrates
- SMIL parser
- file classes for managing indexes, media, downloads, etc.
- system information for reports and inspecting
- generating reports and sends them to a WebDav-Address 
- configuration and loggings tools

## garlic-common

Some visualization classes which are needed for all players

## player-widget

The player view based on Widget-UI-Lib for Windows, BSD, Linux and OS/X.

## player-c2qml

This player-view based on QML controlled by C++ objects. It is regulary used for Android and iOS.

In theory it is possible to use this version also for Windows Linux, but there are some limitatons. E.g.in WebView-component.

## player-qml (useless - deleted)
 
A view concept study based on pure QML technologie with javascript objects. Works only with images. Bugs for video/audio in Qt5.9.x - Do not use it
Look in git history

## player-dummy (obsolte - deleted)

Only a minimal "client" for testing the garlic-lib (e.g. responsibility, memory leaks, etc.)
Look in git history


