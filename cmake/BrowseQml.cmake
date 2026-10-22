function(yalahoo_qml_group output directory)
    set(files)
    foreach(name IN LISTS ARGN)
        set(file "${PROJECT_SOURCE_DIR}/qml/Yalahoo/Music/${directory}/${name}.qml")
        set_source_files_properties("${file}" PROPERTIES QT_RESOURCE_ALIAS "${directory}/${name}.qml")
        list(APPEND files "${file}")
    endforeach()
    set(${output} ${files} PARENT_SCOPE)
endfunction()

function(yalahoo_browse_qml output)
    set(files)
    yalahoo_qml_group(group components StageFocus
        ColumnNavigation TrackRow CreditCard DetailHeader DetailAlbumInfo DetailIntroduction
        DetailTabs HeaderTransport OverviewLabel StageFrameDriver SceneGraphVisual
        ArchiveVectorImage DisplaySurface DesignTokens ArchiveText ArchiveButton ArchiveSlider
        ArchiveProgressBar ArchiveCheckBox ArchiveComboBox ArchiveScrollBar ArchiveScrollView
        ArchiveTextField ArchiveTextArea ArchiveEditorSurface BackdropTexture BackdropBlur
        BackdropBlurPass ArchiveEditMenu ArchiveToolTip ThemeSwitch Header BrowseInfo Footer
        ReadingVeil NavigationFrame Ruler OrdinalReel TitleReel TransportTitle TrackList
        PlaybackControls OverviewBackdrop DetailTabsBackdrop OpeningBackdrop
    )
    list(APPEND files ${group})
    yalahoo_qml_group(group pages Browse
        Detail Overview Opening MusicStage
    )
    list(APPEND files ${group})
    yalahoo_qml_group(group panels StagePanels
        SearchFilters SearchResult AppearanceSettings ArrayQualitySettings SoundSettings
        MetadataSettings ExperimentOverlay SearchPanel LibraryPanel LibraryPanels PanelHeading
        PanelDescription PlaybackPanel SettingsPanel SettingChoice SettingRange MotionSpeedRange
        InterfaceSoundPreview MetadataSection ThemeCard SettingsSection LightingPanel
        MotionPanel ExperimentPanels
    )
    list(APPEND files ${group})
    set(${output} ${files} PARENT_SCOPE)
endfunction()
