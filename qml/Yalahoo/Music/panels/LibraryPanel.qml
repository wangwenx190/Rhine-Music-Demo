pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Dialogs
import Yalahoo.Music
import "../components"

ArchiveScrollView {
    id: root
    required property LibrarySession librarySession
    property Component heading: null
    property LibraryStore store: null
    implicitHeight: 550
    contentWidth: availableWidth
    function focusEditor() {
        roots.forceActiveFocus();
    }
    function loadRoots() {
        roots.text = store ? store.rootPaths().join("\n") : "";
    }
    FolderDialog {
        id: folder
        title: "选择本地音乐文件夹"
        onAccepted: {
            roots.text += (roots.text.trim().length ? "\n" : "") + root.store.localPath(
                        selectedFolder);
        }
    }
    Column {
        width: root.availableWidth
        spacing: 15
        Loader {
            width: parent.width
            sourceComponent: root.heading
        }
        ArchiveText {
            width: parent.width
            tokens: root.tokens
            text: "根目录中的每首单曲各是一张卡片，优先使用自身内嵌封面。子文件夹按专辑展示，优先使用文件夹封面。"
            cssPixelSize: 12
            cssLineHeight: 22.2
            wrapMode: Text.Wrap
            color: root.tokens.muted
        }
        ArchiveText {
            tokens: root.tokens
            text: "音乐文件夹 · 多个目录各占一行"
            cssPixelSize: 11
        }
        ArchiveTextArea {
            id: roots
            tokens: root.tokens
            objectName: "libraryRootsEditor"
            width: parent.width
            height: implicitHeight
            font.family: root.tokens.regular.family
            font.pixelSize: 12
            color: root.tokens.ink
            selectByMouse: true
            wrapMode: TextEdit.WrapAnywhere
            placeholderText: "输入本地绝对路径，或选择文件夹"
            placeholderTextColor: root.tokens.muted
            background: Rectangle {
                color: root.tokens.surface
                border.color: root.tokens.line
            }
            Accessible.name: "音乐文件夹，多个目录各占一行"
        }
        Flow {
            width: parent.width
            spacing: 20
            ArchiveButton {
                tokens: root.tokens
                text: "选择文件夹 ↗"
                enabled: root.store !== null
                onClicked: folder.open()
            }
            ArchiveButton {
                objectName: "librarySaveRoots"
                tokens: root.tokens
                text: "保存目录并扫描 ↗"
                enabled: root.store !== null && !root.store.busy
                onClicked: root.store.setRootPaths(roots.text)
            }
            ArchiveButton {
                tokens: root.tokens
                text: "重新扫描"
                enabled: root.store !== null && !root.store.busy
                onClicked: root.store.rescan()
            }
            ArchiveButton {
                tokens: root.tokens
                text: "取消"
                visible: root.store !== null && root.store.busy
                onClicked: root.store.cancel()
            }
        }
        ArchiveText {
            objectName: "libraryScanStatus"
            tokens: root.tokens
            width: parent.width
            cssPixelSize: 10
            cssLineHeight: 18
            wrapMode: Text.WrapAnywhere
            color: root.store && root.store.error ? root.tokens.accent : root.tokens.muted
            text: !root.store ? "匿名预览不读取本地音乐目录。" : root.store.probing ? "正在读取歌曲标签与封面，已有曲库可以继续浏览…" :
                                                                        root.store.scanning
                                                                        ? "正在扫描，已有曲库可以继续浏览…" :
                                                                          root.store.busy
                                                                          ? "正在读取或保存本地音乐索引…" :
                                                                            root.store.error
                                                                            || root.librarySession.summary.offline
                                                                            || (root.store.roots.length
                                                                                ? "本地索引已就绪。" :
                                                                                  "尚未配置音乐目录。")
            Accessible.role: Accessible.StaticText
        }
        Rectangle {
            width: parent.width
            height: 1
            color: root.tokens.line
        }
        Row {
            width: parent.width
            Repeater {
                model: [
                    {
                        value: root.librarySession.summary.albums,
                        label: "专辑"
                    },
                    {
                        value: root.librarySession.summary.tracks,
                        label: "曲目"
                    },
                    {
                        value: root.librarySession.summary.genres,
                        label: "流派"
                    }
                ]
                delegate: Row {
                    required property var modelData
                    width: parent.width / 3
                    spacing: 12
                    ArchiveText {
                        tokens: root.tokens
                        text: parent.modelData.value.toString()
                        cssPixelSize: 29
                    }
                    ArchiveText {
                        tokens: root.tokens
                        text: parent.modelData.label
                        cssPixelSize: 10
                        color: root.tokens.muted
                        anchors.baseline: parent.children[0].baseline
                    }
                }
            }
        }
        Rectangle {
            width: parent.width
            height: 1
            color: root.tokens.line
        }
        ArchiveText {
            tokens: root.tokens
            text: "封面显示"
            cssPixelSize: 12
        }
        ArchiveText {
            width: parent.width
            tokens: root.tokens
            text: "方形、竖版、横版封面均保持原始比例，完整放入卡片正面。没有封面时显示专辑名称占位，不使用其他专辑的图片。"
            cssPixelSize: 11
            cssLineHeight: 20.9
            color: root.tokens.muted
            wrapMode: Text.Wrap
        }
    }
    Component.onCompleted: loadRoots()
}
