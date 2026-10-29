# garlic-player - SMIL Player for Digital Signage

The garlic-player is a SMIL 3.0 compatible media player that was developed specifically for the requirements of the digital signage industry. 

It is used in numerous projects as well as on various Android hardware solutions.

The garlic-player offers you more independence and security for your investment by being based on the open [SMIL standard](https://garlic-player.com/resources/digital-signage-smil/) standard. It works together with every compatible [digital signage software](https://smil-control.com/software/). This helps also to avoid vendor lock-ins.

[Documentation & Tutorials](https://garlic-player.com/garlic-player/docs/)

[![Raspberry videos](https://img.youtube.com/vi/yNVzrWpZ93U/hqdefault.jpg)](https://youtu.be/yNVzrWpZ93U)

Click on the image to see how a Raspberry Pi 4 playing four videos in zones.

Binaries can be [downloaded](https://garlic-signage.com/garlic-player/downloads/). You will find stable releases and nightly build there.

## Features (master trunk)
 - Support for multiple screens (not on Android)
 - multiple and overlapping zones e.g. splitscreens
 - logging
 - resume downloads
 - 301 redirect support
 - many video formats 
 - supported SVG 1.2. Tiny Profile, Jpg, Png and Gif
 - media cache
 - SMIL3 subset, HTML5, HTML5-Widgets and (depending on device) WebGL support
 - WebWidgets with also support parameters
 - Webserver on port 8080 for Restful Api

## Official Operating Systems Supported
 - Linux
 - Windows
 - Android
 - macOS
 - iOS in testing: https://testflight.apple.com/join/1Y79ryHC if you want to join
 
### Successfully Tested With:
 - Linux 64Bit: Kde Neon, Kubuntu, Debian, Arch64
 - Raspberry Pi: 3, 4, and 5 for 32 and 64Bit
 - Tinker Board: (Linaro OS 1.8)
 - Android Armv7 (4.4.1, 5.0.1, 5.1.1, 6.0.2, 7.1, 9 and 10)
 - Windows 64Bit: 7, 8, 10, and 11
 - MacOS 64Bit (Intel): High Sierra+
 - MacOS M1: Big Sur+
 - iOS 14.4.2 (experimental)


## Command Line Options

| Option | Description |
|-------------------|----------------------------------------------------------------|
| `-h, --help` | Displays help |
| `-v, --version` | Displays version information |
| `-c, --config` | Path to `config.xml` |
| `-m, --windows-mode` | Full screen, big screen or window mode |
| `-z, --windows-size` | Window size in window mode, e.g. `980x540` |
| `-s, --screen-select` | Starts in the selected screen |

Arguments:
SMIL_INDEX        Path to SMIL index

For Android and iOS player-c2qml is needed, cause only QML based multimedia is supported on this operating systems.

*garlic.sh [options] SMIL_INDEX*

## HotKeys
 - toggle fullscreen with Ctrl-F (not in Android)
 - toogle bigscreen with Ctrl-B (not in Android)
 - quit app with Ctrl-Q
 - open configuration dialog with Ctrl-S

## Currently supported SMIL-Features

### Time container elements
- **seq**
- **par**
- **excl**
- **priorityClass**

#### Attributes for excl/priorityClass time container
- **peers** stop|never|pause|defer
- **higher** stop|pause
- **lower** never|defer

### Media-Elements
- **video**
- **audio**
- **images**
- **brush**
- **html/text**
- **HTML Widgets**

### Other Elements
- **prefetch**
- **meta** for refresh index via http-equiv="Refresh"

#### Attributes for par and excl
- **endsync** first|last|id-value

#### Attributes for media
- **region**
- **fit** fill|meet|meetbest

#### Attributes for mbrush
- **color** color in CSS Style

### Attributes for media and container (Timings and Synchronization)
- **xml:id** with Fallback for SMIL 2.0 id
- **title** stub
- **restart** (always|never|whenNotActive|default)
- **class** stub
- **xml:lang** stub
- **repeatCount** (currently only positive integer values)

#### Begin and End Attributes
- **Clock values** like dur
- **Wallclock** with weekday (-/+) periods and repeats. Date is supported in full ISO 8601 format not short date like 201634W.
- **Events** beginEvent, endEvent, activateEvent with additional clock value like id.endEvent+2s
- **Syncbase values** with additional clock value like id.begin+2s
- **accesskey** with additional clock value like accesskey('w')+2s
- **indefinite**
- **value lists** seperated by ";"

#### Timings with Dur
- **indefinite**
- **media**
- **clock values** (full+partial clock),  normal (h min s and ms)

### Layered multizone layouts elements:
- **root-layout**
- **layout**

### Multizone attributes
- **regionName**
- **top** values in px or %
- **left** values in px or %
- **width** values in px or %
- **height** values in px or %
- **z-index** integer values
- **backgroundColor** values are compatible with CSS style
- **backgroundImage** png and jpg images (not in android player)
- **backgroundRepeat**  values: repeat | repeatX | repeatY | noRepeat (not in android player)

<!-- -->  HTML-like comments in SMIL-File are supported and content inside will ignored

## additional features supported not in w3c standard
 - **shuffle play** in seq container via metadata
 - **automatic refresh** via metatags in SMIL header
 - **Logs/reports** system and inventory-reports, events- and playlogs via subscription meta and delivery by webdav described in [Reporting](https://garlic-signage.com/garlic-player/docs/essentials/logs-reports/)
 - **expr-attribute** in media tags [conditional play](https://garlic-signage.com/garlic-player/docs/essentials/conditional-play/)
 - notify network trigger for begin and end attributes
 - REST-API ([documentation](https://garlic-signage.com/garlic-player/docs/rest-api/))
 - placeholer in head with status and data tags
 - http downloads and caching
 - caching of remote SMIL index file
 - caching of remote media files
 - refresh via meta tag in cyclic timer periods
 - http 301 redirects for index and media
 - change config.xml during playback
 - remote update

Contact: [Niko Sagiadinos] (mailto:garlic@sagiadinos.de)
