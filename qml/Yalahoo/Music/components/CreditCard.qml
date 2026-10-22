// Displays one credit group; the nested track list stays bounded by the detail viewport.
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Templates as T
import Yalahoo.Music

Column {
    id: person
    required property DesignTokens tokens
    required property var credit
    required property real viewportHeight
    readonly property real textSize: 11
    readonly property real popupHeightRatio: 0.7
    readonly property real trackRowPixels: 80

    objectName: "detailCredit"
    property bool expanded: false
    onCreditChanged: expanded = false
    spacing: 4
    bottomPadding: 18
    ArchiveText {
        tokens: person.tokens
        width: parent.width
        text: person.credit.name
        cssPixelSize: 14
        cssLineHeight: cssPixelSize * 1.6
        wrapMode: Text.WrapAnywhere
    }
    ArchiveText {
        tokens: person.tokens
        width: parent.width
        text: person.credit.rolesText
        cssPixelSize: 12
        cssLineHeight: cssPixelSize * 1.7
        wrapMode: Text.WrapAnywhere
    }
    ArchiveText {
        tokens: person.tokens
        width: parent.width
        text: qsTranslate("TrackList", "Source: %1").arg(person.credit.sourcesText)
        cssPixelSize: 10
        cssLineHeight: cssPixelSize * 1.5
        wrapMode: Text.WrapAnywhere
        color: person.tokens.muted
    }
    ArchiveButton {
        objectName: "detailCreditExpand"
        tokens: person.tokens
        visible: person.credit.tracks.length > 1
        text: (person.expanded ? qsTranslate("TrackList", "Hide credited tracks \u2212") :
                                 qsTranslate("TrackList", "Show credited tracks +"))
        cssPixelSize: person.textSize
        onClicked: person.expanded = !person.expanded
        Accessible.expanded: person.expanded
    }
    ArchiveText {
        tokens: person.tokens
        width: parent.width
        visible: person.credit.tracks.length === 1
        text: person.credit.tracks.length === 1 ? person.credit.tracks[0] : ""
        cssPixelSize: person.textSize
        cssLineHeight: cssPixelSize * 1.7
        wrapMode: Text.WrapAnywhere
    }
    ListView {
        visible: person.expanded && person.credit.tracks.length > 1
        width: parent.width
        height: visible ? Math.min(person.viewportHeight * person.popupHeightRatio,
                                   person.credit.tracks.length * person.trackRowPixels) : 0
        clip: true
        reuseItems: true
        cacheBuffer: 100
        boundsBehavior: Flickable.StopAtBounds
        model: visible ? person.credit.tracks : []
        delegate: ArchiveText {
            required property string modelData
            tokens: person.tokens
            width: ListView.view.width
            text: modelData
            cssPixelSize: person.textSize
            cssLineHeight: cssPixelSize * 1.7
            bottomPadding: 12
            wrapMode: Text.WrapAnywhere
        }
        T.ScrollBar.vertical: ArchiveScrollBar {
            tokens: person.tokens
            policy: T.ScrollBar.AsNeeded
        }
    }
    ArchiveText {
        tokens: person.tokens
        width: parent.width
        visible: person.credit.unscopedText.length > 0
        text: qsTranslate("TrackList", "Credits without track assignments \u00b7 %1").arg(
                  person.credit.unscopedText)
        cssPixelSize: person.textSize
        cssLineHeight: cssPixelSize * 1.7
        wrapMode: Text.WrapAnywhere
        color: person.tokens.muted
    }
    Rectangle {
        antialiasing: true
        width: parent.width
        height: 1
        color: person.tokens.line
    }
}
