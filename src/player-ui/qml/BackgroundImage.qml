// Background image of the root layout or a region with backgroundRepeat of the SMIL (repeat, repeatX, repeatY,
// noRepeat), like CSS background-repeat: the tiles keep the size of the image. Image.TileHorizontally/TileVertically
// would stretch them to the other side, so repeatX/repeatY tile a strip one image high or wide instead.
// The parent clips what lies outside.
import QtQuick

Image {
    property string repeat: "repeat" // lower case

    anchors.left: parent.left
    anchors.top: parent.top
    width: repeat === "repeaty" || repeat === "norepeat" ? implicitWidth : parent.width
    height: repeat === "repeatx" || repeat === "norepeat" ? implicitHeight : parent.height
    visible: source.toString() !== ""
    fillMode: repeat === "norepeat" ? Image.Pad : Image.Tile
    horizontalAlignment: Image.AlignLeft
    verticalAlignment: Image.AlignTop
}
