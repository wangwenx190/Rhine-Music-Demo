"""Review shared control ownership/cost paths; target runtime evidence is independent."""
from audit_support import BASELINE, file_dependencies, historical, source_rows, write_source_report
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CHECKS = [
    ("COMMON-01", "cmake/QtKit.cmake", "QuickTemplates2",
     "Latest native Kit discovery requires QuickTemplates2. Product and Review link its public target; all QML resources share the root C++23 module closure without platform-specific control implementations."),
    ("COMMON-02", "cmake/BrowseQml.cmake", "ArchiveTextArea",
     "Shared Archive files receive module-relative resource aliases in product and Review. Fonts and icon URLs resolve existing qrc assets; text edit actions do not scan or write music paths. FolderDialog remains the OS chooser."),
    ("COMMON-03", "qml/Yalahoo/Music/components/ArchiveSlider.qml", "control.visualPosition",
     "Qt logical item geometry maps position/mirroring into explicit source control extents. Rectangle/Shape/Text render through the selected scene graph backend; no shader/pass/format/camera or target precision changes."),
    ("COMMON-04", "qml/Yalahoo/Music/panels/PlaybackPanel.qml", "onMoved: root.playback.volume = value",
     "Volume/seek/device widgets retain existing public PlaybackController routes. Visual replacement adds no audio object, engine, media IO or frame-based playback loop; navigation and panel ownership stay separate."),
    ("COMMON-05", "tools/review/ControlsAudit.cpp", 'check(u"unicode-ime-selection"_s',
     "ControlsReview checks actual Qt key/IME/selection/undo semantics and source geometry in its own render target. Host panel tests exercise production trees. Hidden results cannot certify physical input, OS accessibility or another target."),
    ("PERF-01", "qml/Yalahoo/Music/components/ArchiveComboBox.qml", "optionsWidth",
     "Natural width measures only the bounded choice model on model/font change. Library result virtual views remain in their existing owners; no library-wide traversal or per-frame text measurement was added."),
    ("PERF-02", "src/render/text/TextAreaFormat.cpp", "document->findBlock(position)",
     "GUI-side text formatting visits only changed paragraphs on editing, with a reentrancy guard and shared QTextDocument lifetime. No worker/queued backlog or secondary input state machine is added."),
    ("PERF-03", "qml/Yalahoo/Music/panels/SettingRange.qml", "onMoved: root.moved(value)",
     "Control input forwards one scalar into existing setting routes; equality/normalization/coalesced preference ownership remains in controllers. No cover copy/upload or scene rebuild is introduced."),
    ("PERF-04", "qml/Yalahoo/Music/components/ArchiveScrollBar.qml", "contentItem: Rectangle",
     "Primitive scene graph visuals add no texture layer, capture/readback, blur pass, offscreen target or render callback. Scroll bars attach to their owning Flickable; only visible controls render."),
    ("PERF-05", "qml/Yalahoo/Music/components/ArchiveComboBox.qml", "control.popup.visible ? control.delegateModel : null",
     "Popup delegates exist only while open, use ListView virtualization and zero highlight animation. Fonts use existing registered family/style; project tokens drive theme invalidations."),
    ("PERF-06", "qml/Yalahoo/Music/components/ArchiveEditMenu.qml", "menu.editor[modelData.action]()",
     "Qt owns editing/clipboard/selection and Templates owns focus/accessibility. Four bounded menu actions use Qt events and do not establish a frame source/timer, polling or JS serialization loop."),
    ("PERF-07", "src/render/text/TextAreaFormat.cpp", "disconnect(m_connection)",
     "Text helper disconnects document replacement and uses QPointer/reentrancy protection. Popup Item ownership follows its QML tree; closing panels destroys controls/delegates without persistent render resources. Native lifecycle/physical requirements remain UI-CONTROLS-P."),
]
paths = {str(p.relative_to(ROOT)) for base in ("qml", "tools/review", "resources")
         for p in (ROOT / base).rglob("*.qml")}
paths |= {path for _, path, _, _ in CHECKS}
paths |= {"src/app/CMakeLists.txt", "src/app/QmlTypes.h", "src/app/main.cpp",
          "src/render/text/TextAreaFormat.h", "src/render/text/CMakeLists.txt",
          "tools/review/CMakeLists.txt", "tests/integration/CMakeLists.txt",
          "cmake/TestPolicy.cmake", "docs/contracts/controls.md"}
dependencies = file_dependencies(ROOT, sorted(paths))
revision = json.loads((ROOT / "docs/contracts/history-rewrite.json").read_text())["revisions"][BASELINE]
frozen = [historical(revision, path) for path in ("src/style.css", "src/music.css", "src/music-lighting-lab.css")]
for platform, rule in (("windows", "WIN-01"), ("macos", "MAC-01"), ("linux", "LNX-01")):
    platform_check = (rule, "cmake/Platform.cmake", "APPLE" if platform == "macos" else "WIN32" if platform == "windows" else "UNIX",
                      "Platform bootstrap selects its native window/backend and module closure. Shared control input/visual code has no target omissions; actual compiler/plugins/physical input/IME/accessibility and window/DPR behavior require UI-CONTROLS-P on that system.")
    checks = source_rows(ROOT, CHECKS + [platform_check],
                       platform=platform, result='source-reviewed', limitations='Target runtime/physical evidence remains separate under UI-CONTROLS-P.')
    write_source_report(ROOT, platform, 'controls', dict(checks=checks, dependencies=dependencies, frozen=frozen))
print("Control COMMON/PERF/platform source bindings verified; runtime remains separate")
