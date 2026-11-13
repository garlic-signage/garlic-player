// The window of the QML player: regions of the current layout, the menu, the dialogs and the shortcuts. Locked by the
// supervisor (Player.locked) only the accesskeys of the SMIL work.
// The same behaviour as the qwidget Player. "player" is the C++ bridge to the core (Player.hpp).
import QtQuick
import Garlic
import QtQuick.Controls

ApplicationWindow {
    id: window
    title: "garlic-player"
    color: Player.rootColor

    // background of the root layout: color and image (repeat, repeatX, repeatY, noRepeat)
    Image {
        anchors.fill: parent
        source: Player.rootImage
        visible: source.toString() !== ""
        fillMode: {
            switch (Player.rootRepeat) {
            case "repeatx": return Image.TileHorizontally
            case "repeaty": return Image.TileVertically
            case "norepeat": return Image.Pad
            default: return Image.Tile
            }
        }
        horizontalAlignment: Image.AlignLeft
        verticalAlignment: Image.AlignTop
    }

    Item {
        id: stage
        anchors.fill: parent
        focus: true

        Repeater {
            model: Player.regions
            delegate: RegionView {}
        }

        // every key without modifier is an accesskey for the SMIL
        Keys.onPressed: (event) => {
            if (event.modifiers === Qt.NoModifier && event.text.length > 0) {
                Player.accessKey(event.text)
                event.accepted = true
            }
        }
    }

    Shortcut { sequence: "Ctrl+F"; enabled: !Player.locked; onActivated: Player.toggleFullScreen() }
    Shortcut { sequence: "Ctrl+B"; enabled: !Player.locked; onActivated: Player.showOverAllScreens() }
    Shortcut { sequence: "Ctrl+S"; enabled: !Player.locked; onActivated: settingsDialog.open() }
    Shortcut { sequence: "Ctrl+D"; enabled: !Player.locked; onActivated: debugDialog.open() }
    Shortcut { sequence: "Ctrl+Q"; enabled: !Player.locked; onActivated: Player.quit() }

    // ten fast clicks or one held for 10 s (counted in C++ on the window, not while locked)
    Connections {
        target: Player
        function onMenuRequested(position) { menu.popup(position.x, position.y) }
        // locked by the supervisor: what is open closes, the shortcuts are off
        function onLockedChanged() {
            if (Player.locked) {
                menu.close()
                settingsDialog.close()
                debugDialog.close()
            }
        }
    }

    PlayerMenu {
        id: menu
        onSettingsChosen: settingsDialog.open()
        onDebugChosen: debugDialog.open()
    }
    SettingsDialog { id: settingsDialog }
    DebugDialog { id: debugDialog }

    // without an index nothing can be played: the settings first, cancelled ends the player (garlic-lib);
    // not while locked
    Component.onCompleted: if (Player.needsSettings && !Player.locked) settingsDialog.openAtStart()
}
