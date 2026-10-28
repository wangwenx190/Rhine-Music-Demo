// Presents saved album introduction, provenance and credit status without querying providers.
pragma ComponentBehavior: Bound
import QtQuick
import Yalahoo.Music

Column {
    id: root
    required property DesignTokens tokens
    required property var album

    spacing: 14
    topPadding: 27
    bottomPadding: 20
    ArchiveText {
        tokens: root.tokens
        text: qsTranslate("Detail", "ABOUT THIS ALBUM")
        cssPixelSize: 8
        color: root.tokens.muted
    }
    ArchiveText {
        tokens: root.tokens
        width: parent.width
        objectName: "detailDescription"
        text: root.album.description || qsTranslate("Detail", "Album introduction pending")
        cssPixelSize: 12
        cssLineHeight: cssPixelSize * 1.95
        wrapMode: Text.Wrap
    }
    ArchiveText {
        tokens: root.tokens
        width: parent.width
        text: root.album.descriptionSourceText || ""
        cssPixelSize: 9
        wrapMode: Text.WrapAnywhere
        color: root.tokens.muted
    }
    ArchiveText {
        objectName: "detailIntroductionStatus"
        tokens: root.tokens
        width: parent.width
        text: root.album.introductionStatus || ""
        cssPixelSize: 11
        cssLineHeight: cssPixelSize * 1.9
        wrapMode: Text.Wrap
        color: root.tokens.muted
        Accessible.role: Accessible.StaticText
    }
    ArchiveText {
        tokens: root.tokens
        width: parent.width
        text: qsTranslate("Detail", "Local note\n%1").arg(root.album.localNote || qsTranslate(
                                                              "Detail", "Not provided"))
        cssPixelSize: 11
        wrapMode: Text.Wrap
    }
    ArchiveText {
        tokens: root.tokens
        width: parent.width
        text: qsTranslate("Detail", "Local directory\n%1\n%2").arg(root.album.folder || "").arg(
                  root.album.rawGenresText || "")
        cssPixelSize: 9
        wrapMode: Text.WrapAnywhere
        color: root.tokens.muted
    }
    Column {
        width: parent.width
        spacing: 6
        topPadding: 24
        ArchiveText {
            tokens: root.tokens
            text: qsTranslate("Detail", "ALBUM CREDITS")
            cssPixelSize: 9
            font.letterSpacing: 1.4
            color: root.tokens.muted
        }
        ArchiveText {
            tokens: root.tokens
            text: qsTranslate("Detail", "Contributors")
            cssPixelSize: 16
            font.letterSpacing: 0.4
        }
    }
    ArchiveText {
        objectName: "detailCreditsStatus"
        tokens: root.tokens
        width: parent.width
        text: root.album.creditsStatus || qsTranslate("Detail", "No credits available.")
        cssPixelSize: 11
        cssLineHeight: cssPixelSize * 1.9
        wrapMode: Text.Wrap
        color: root.tokens.muted
    }
}
