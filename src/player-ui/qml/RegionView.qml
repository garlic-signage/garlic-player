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

    /// backgroundRepeat of the SMIL → fill mode of an Image (garlic-lib: CSS of the region)
    function tiling(repeat) {
        switch (repeat) {
        case "repeatx": return Image.TileHorizontally
        case "repeaty": return Image.TileVertically
        case "norepeat": return Image.Pad
        default: return Image.Tile
        }
    }

    Rectangle {
        anchors.fill: parent
        color: region.model.color
    }
    Image {
        anchors.fill: parent
        source: region.model.image
        visible: source.toString() !== ""
        fillMode: region.tiling(region.model.repeat)
        horizontalAlignment: Image.AlignLeft
        verticalAlignment: Image.AlignTop
    }

    Repeater {
        model: region.model.media
        delegate: MediaView {}
    }
}
