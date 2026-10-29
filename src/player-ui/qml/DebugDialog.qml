// Every second the resources from the core and the media that play now, one line per region.
import QtQuick
import Garlic
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: dialog
    title: "Information"
    modal: true
    anchors.centerIn: Overlay.overlay
    width: Math.min(680, Overlay.overlay ? Overlay.overlay.width - 32 : 680)
    closePolicy: Popup.CloseOnEscape

    property var info: ({})
    property var playing: []

    function refresh() {
        info = Player.debugInfo()
        playing = Player.playingMedia()
    }

    onAboutToShow: { refresh(); Player.showCursor(true) }
    onClosed: Player.showCursor(false)

    Timer {
        interval: 1000
        repeat: true
        running: dialog.visible
        onTriggered: dialog.refresh()
    }

    contentItem: GridLayout {
        columns: 2
        columnSpacing: 12
        Label { text: "Playlist" }
        Label { text: dialog.info.title ?? "-"; wrapMode: Text.Wrap; Layout.fillWidth: true }
        Label { text: "User agent" }
        Label { text: dialog.info.userAgent ?? "-"; wrapMode: Text.WrapAnywhere; Layout.fillWidth: true }
        Label { text: "Disk (cache)" }
        Label { text: dialog.info.disk ?? "-"; Layout.fillWidth: true }
        Label { text: "System memory" }
        Label { text: dialog.info.memory ?? "-"; Layout.fillWidth: true }
        Label { text: "Player memory" }
        Label { text: dialog.info.playerMemory ?? "-"; Layout.fillWidth: true }
        Label { text: "Threads" }
        Label { text: dialog.info.threads ?? "-"; Layout.fillWidth: true }
        Label { text: "Playing"; Layout.alignment: Qt.AlignTop }
        Label { text: dialog.playing.length > 0 ? dialog.playing.join("\n") : "-"; wrapMode: Text.Wrap; Layout.fillWidth: true }
    }

    footer: RowLayout {
        Button { text: "Quit player"; onClicked: Qt.quit(); Layout.leftMargin: 12; Layout.bottomMargin: 12 }
        Item { Layout.fillWidth: true }
        Button { text: "Close"; highlighted: true; onClicked: dialog.close(); Layout.rightMargin: 12; Layout.bottomMargin: 12 }
    }
}
