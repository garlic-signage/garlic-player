// One <region>: placed relative to the window (0..1), background color and image, its media stacked (the newest on
// top, only the topmost one with a picture is visible).
pragma ComponentBehavior: Bound
import QtQuick

Item {
    id: region
    required property var model // roles of RegionModel: left, top, width, height, color, image, repeat, media

    x: Math.round(parent.width * model.left)
    y: Math.round(parent.height * model.top)
    width: Math.round(parent.width * model.width)
    height: Math.round(parent.height * model.height)
    clip: true

    Rectangle {
        anchors.fill: parent
        color: region.model.color
    }
    BackgroundImage {
        source: region.model.image
        repeat: region.model.repeat
    }

    Repeater {
        model: region.model.media
        delegate: MediaView {}
    }
}
