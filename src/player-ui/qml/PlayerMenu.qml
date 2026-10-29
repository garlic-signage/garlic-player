// The menu of all players: Settings, Debug infos, Exit, with the shortcuts right-aligned.
import QtQuick
import Garlic
import QtQuick.Controls
import QtQuick.Layouts

Menu {
    id: menu
    signal settingsChosen()
    signal debugChosen()

    onAboutToShow: Player.showCursor(true)
    onAboutToHide: Player.showCursor(false)

    component Entry: MenuItem {
        id: entry
        property string keys
        contentItem: RowLayout {
            spacing: 32
            Label { text: entry.text; Layout.fillWidth: true }
            Label { text: entry.keys; opacity: 0.6; horizontalAlignment: Text.AlignRight }
        }
    }

    Entry { text: "Settings"; keys: "Ctrl+S"; onTriggered: menu.settingsChosen() }
    Entry { text: "Debug infos"; keys: "Ctrl+D"; onTriggered: menu.debugChosen() }
    Entry { text: "Exit"; keys: "Ctrl+Q"; onTriggered: Qt.quit() }
}
