// One medium in one region, from StartMedia to StopMedia. The timing is the business of the core: a medium only shows
// itself and reports what only the frontend knows (the end of a video, a file that cannot be played, a click).
pragma ComponentBehavior: Bound
import QtQuick
import Garlic
import QtMultimedia
import QtWebView

Item {
    id: media
    required property int instance
    required property string kind     // image, video, audio, web, brush
    required property string source
    required property string fit      // fill, meet, meetbest, slice, else the original size
    required property string align    // mediaAlign
    required property real volume
    required property color color
    required property bool primary    // only the first region of an instance reports end and errors
    required property bool paused
    required property int generation  // RepeatMedia: again from the beginning
    required property bool onTop

    anchors.fill: parent
    visible: onTop || kind === "audio"

    function finished() { if (primary) Player.mediaFinished(instance) }
    function failed(message) { if (primary) Player.mediaFailed(instance, message) }

    function horizontal() {
        if (align.endsWith("right")) return Qt.AlignRight
        if (align === "center" || align.endsWith("mid")) return Qt.AlignHCenter
        return Qt.AlignLeft
    }
    function vertical() {
        if (align.startsWith("bottom")) return Qt.AlignBottom
        if (align === "center" || align.startsWith("mid")) return Qt.AlignVCenter
        return Qt.AlignTop
    }

    Loader {
        anchors.fill: parent
        sourceComponent: {
            switch (media.kind) {
            case "image": return imageView
            case "video": return videoView
            case "audio": return audioView
            case "web": return webView
            default: return brushView
            }
        }
    }

    // a click on a medium (web pages handle their clicks themselves)
    MouseArea {
        anchors.fill: parent
        enabled: media.kind !== "web" && media.onTop
        onClicked: Player.activated(media.instance)
    }

    Component {
        id: imageView
        Image {
            source: media.source
            asynchronous: true
            clip: true
            horizontalAlignment: media.horizontal()
            verticalAlignment: media.vertical()
            fillMode: {
                switch (media.fit) {
                case "fill": return Image.Stretch
                case "meet": return Image.PreserveAspectFit
                case "slice": return Image.PreserveAspectCrop
                case "meetbest": // meet, but never larger than the original
                    return implicitWidth > width || implicitHeight > height ? Image.PreserveAspectFit : Image.Pad
                default: return Image.Pad
                }
            }
            onStatusChanged: if (status === Image.Error) media.failed("cannot load image " + source)
        }
    }

    Component {
        id: videoView
        Item {
            MediaPlayer {
                id: video
                property bool running: false // one end per run, none after the stop
                source: media.source
                videoOutput: output
                audioOutput: AudioOutput { volume: media.volume }
                onMediaStatusChanged: {
                    if (mediaStatus === MediaPlayer.EndOfMedia && running) { running = false; media.finished() }
                    else if (mediaStatus === MediaPlayer.InvalidMedia && running) { running = false; media.failed("invalid media") }
                }
                onErrorOccurred: (error, message) => { if (running) { running = false; media.failed(message) } }
                Component.onCompleted: { running = true; play() }
            }
            VideoOutput {
                id: output
                anchors.fill: parent
                fillMode: media.fit === "fill" ? VideoOutput.Stretch
                        : media.fit === "slice" ? VideoOutput.PreserveAspectCrop : VideoOutput.PreserveAspectFit
            }
            Connections {
                target: media
                function onPausedChanged() { media.paused ? video.pause() : video.play() }
                function onGenerationChanged() { video.running = true; video.position = 0; video.play() }
            }
        }
    }

    Component {
        id: audioView
        Item {
            MediaPlayer {
                id: audio
                property bool running: false
                source: media.source
                audioOutput: AudioOutput { volume: media.volume }
                onMediaStatusChanged: {
                    if (mediaStatus === MediaPlayer.EndOfMedia && running) { running = false; media.finished() }
                    else if (mediaStatus === MediaPlayer.InvalidMedia && running) { running = false; media.failed("invalid media") }
                }
                onErrorOccurred: (error, message) => { if (running) { running = false; media.failed(message) } }
                Component.onCompleted: { running = true; play() }
            }
            Connections {
                target: media
                function onPausedChanged() { media.paused ? audio.pause() : audio.play() }
                function onGenerationChanged() { audio.running = true; audio.position = 0; audio.play() }
            }
        }
    }

    Component {
        id: webView
        WebView {
            id: web
            url: media.source
            visible: !media.paused
            // like garlic-lib: widgets read local files and remote data
            settings.javaScriptEnabled: true
            settings.localStorageEnabled: true
            settings.allowFileAccess: true
            settings.localContentCanAccessFileUrls: true
            onLoadingChanged: (request) => {
                if (request.status === WebView.LoadSucceededStatus)
                    web.runJavaScript("document.body.style.overflow = 'hidden';")
            }
        }
    }

    Component {
        id: brushView
        Rectangle { color: media.color }
    }
}
