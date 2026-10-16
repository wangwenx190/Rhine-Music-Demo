pragma ComponentBehavior: Bound
import QtQuick
import Yalahoo.Music
import "../components"

Column {
    id: root
    required property DesignTokens tokens
    required property LibrarySession librarySession
    required property string kind
    required property string title
    required property string explanation
    readonly property var result: librarySession.metadataFor(librarySession.navigationAlbumId, kind,
                                                             librarySession.metadataRevision)
    readonly property var progress: librarySession.metadataProgress
    readonly property bool queryEnabled: librarySession.providerCapabilities.networkEnabled
                                         && librarySession.providerCapabilities[kind
                                                                                === "introduction"
                                                                                ? "introductions" :
                                                                                  kind]
    width: parent ? parent.width : 300
    spacing: 12
    ArchiveText {
        tokens: root.tokens
        text: root.title
        cssPixelSize: 12
    }
    ArchiveText {
        tokens: root.tokens
        width: parent.width
        text: root.explanation
        cssPixelSize: 12
        color: root.tokens.muted
        wrapMode: Text.Wrap
    }
    ArchiveText {
        objectName: root.kind + "MetadataState"
        tokens: root.tokens
        width: parent.width
        text: root.librarySession.navigationAlbumId ? "当前专辑 · "
                                                      + root.librarySession.browseInfo.title
                                                      + "\n查询状态：" + (root.result.status
                                                                     || "Deferred") + (
                                                          root.result.checkedAt ? "\n检查时间："
                                                                                  + root.result.checkedAt :
                                                                                  "") + (root.result.retryAt
                                                                                         ? "\n重试时间："
                                                                                           + root.result.retryAt :
                                                                                           "") + (root.result.error
                                                                                                  ? "\n" + root.result.error :
                                                                                                    "") : "选择专辑后查看本机资料。"
        cssPixelSize: 12
        wrapMode: Text.WrapAnywhere
        Accessible.role: Accessible.StaticText
    }
    ArchiveText {
        objectName: root.kind + "MetadataContent"
        tokens: root.tokens
        width: parent.width
        text: {
            const payload = root.result.payload || {};
            if (root.kind === "introduction")
                return payload.description || "暂无本机介绍缓存";
            if (root.kind === "credits")
                return (payload.producers || []).map(person => person.name + (person.role ? " · "
                                                                                            + person.role :
                                                                                            "") + (person.source
                                                                                                   ? " · " + JSON.stringify(
                                                                                                         person.source) :
                                                                                                     "")).join(
                            "\n") || "暂无本机制作信息缓存";
            const online = payload.online || {};
            return [online.title || online.albumTitle || "", online.releaseId || "", online.releaseGroupId
                    || ""].filter(value => value.length > 0).join("\n") || "暂无本机 MusicBrainz 标识缓存";
        }
        cssPixelSize: 12
        wrapMode: Text.WrapAnywhere
    }
    ArchiveText {
        tokens: root.tokens
        width: parent.width
        text: "来源：" + (root.result.source ? Object.values(root.result.source).join(" · ") : "")
        cssPixelSize: 11
        color: root.tokens.muted
        wrapMode: Text.WrapAnywhere
    }
    ArchiveButton {
        id: queryButton
        objectName: root.kind + "MetadataQuery"
        tokens: root.tokens
        text: root.kind === "introduction" ? "查询 / 更新专辑介绍 ↗" : root.kind === "credits"
                                             ? "查询缺失的制作信息 ↗" : "补充缺失的在线资料 ↗"
        enabled: root.queryEnabled && !root.progress.active
        onClicked: root.librarySession.requestMetadataBatch(root.kind)
        ArchiveToolTip {
            parent: queryButton
            tokens: root.tokens
            visible: queryButton.hovered && !queryButton.enabled
            text: "联网资料功能暂未启用"
        }
        Accessible.description: enabled ? "" : "联网资料功能暂未启用"
    }
    ArchiveText {
        tokens: root.tokens
        text: "联网资料功能暂未启用；已有缓存可离线查看。"
        width: parent.width
        wrapMode: Text.Wrap
        visible: !root.queryEnabled
        cssPixelSize: 11
        color: root.tokens.muted
    }
    ArchiveProgressBar {
        tokens: root.tokens
        implicitHeight: root.kind === "introduction" ? 3 : 16
        width: parent.width
        visible: root.progress.kind === root.kind && root.progress.total > 0
        from: 0
        to: root.progress.total || 1
        value: root.progress.completed || 0
        Accessible.name: root.title + "查询进度"
    }
    ArchiveText {
        tokens: root.tokens
        visible: root.progress.kind === root.kind && root.progress.total > 0
        text: (root.progress.completed || 0) + " / " + (root.progress.total || 0) + " · " + (
                  root.progress.status || "") + " · 错误 " + (root.progress.errors || 0)
        cssPixelSize: 11
    }
    ArchiveButton {
        tokens: root.tokens
        text: "取消查询"
        visible: root.progress.kind === root.kind && root.progress.active
        onClicked: root.librarySession.cancelAllMetadata()
    }
}
