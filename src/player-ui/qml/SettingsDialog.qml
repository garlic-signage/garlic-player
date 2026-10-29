// Player name, content URL, HTTP server on/off and port. Reading, checking and storing is done by the core; the
// dialog closes only when the core has stored them, otherwise it shows the error text of the core.
import QtQuick
import Garlic
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: dialog
    title: "garlic-player settings"
    modal: true
    anchors.centerIn: Overlay.overlay
    width: Math.min(600, Overlay.overlay ? Overlay.overlay.width - 32 : 600)
    closePolicy: Popup.CloseOnEscape

    property bool atStart: false // at the start without index: cancelled ends the player
    property bool stored: false

    function openAtStart() { atStart = true; open() }

    onAboutToShow: {
        const current = Player.settings()
        playerName.text = current.playerName
        indexUri.text = current.indexUri
        httpdEnabled.checked = current.httpdEnabled
        httpdPort.value = current.httpdPort
        error.text = ""
        stored = false
        Player.showCursor(true)
    }
    onClosed: {
        Player.showCursor(false)
        if (atStart && !stored)
            Qt.exit(1)
        atStart = false
    }

    function store() {
        const message = Player.changeSettings({
            playerName: playerName.text,
            indexUri: indexUri.text,
            httpdEnabled: httpdEnabled.checked,
            httpdPort: httpdPort.value
        })
        if (message !== "") {
            error.text = message
            return
        }
        stored = true
        close()
    }

    contentItem: ColumnLayout {
        spacing: 12
        Label {
            id: error
            visible: text !== ""
            color: "red"
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
        GridLayout {
            columns: 2
            columnSpacing: 12
            Layout.fillWidth: true
            Label { text: "Player name" }
            TextField { id: playerName; Layout.fillWidth: true }
            Label { text: "Content URL" }
            TextField { id: indexUri; Layout.fillWidth: true; placeholderText: "e.g. https://indexes.smil-control.com" }
            Label { text: "HTTP server" }
            CheckBox { id: httpdEnabled; text: "active" }
            Label { text: "Port" }
            SpinBox { id: httpdPort; from: 1; to: 65535; editable: true; enabled: httpdEnabled.checked }
        }
    }

    footer: RowLayout {
        Item { Layout.fillWidth: true }
        Button { text: "OK"; highlighted: true; onClicked: dialog.store() }
        Button { text: "Cancel"; onClicked: dialog.close(); Layout.rightMargin: 12; Layout.bottomMargin: 12 }
    }
}
