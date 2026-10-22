pragma ComponentBehavior: Bound
import QtQuick
import "../components"

ArchiveText {
    id: root
    width: parent ? parent.width : 0
    cssPixelSize: 11
    color: root.tokens.muted
    wrapMode: Text.Wrap
}
