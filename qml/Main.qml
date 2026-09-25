import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs as NativeDialogs
import Blockyard 1.0

ApplicationWindow {
    id: window
    width: 1200
    height: 720
    minimumWidth: 620
    minimumHeight: 480
    visible: true
    title: "Blockyard"
    color: theme.background
    font.family: theme.fontFamily
    font.pixelSize: 13
    palette.window: theme.background
    palette.windowText: theme.foreground
    palette.base: theme.background
    palette.text: theme.foreground
    palette.button: theme.surface
    palette.buttonText: theme.foreground
    palette.highlight: theme.selection
    palette.highlightedText: theme.foreground
    property bool listVisible: false
    property bool compact: width < 900
    readonly property var colors: theme
    property var reviewData: ({ items: [], total: 0, error: "", warnings: [] })
    property var cleanupOutcomes: []
    property bool dialogActive: reviewDialog.visible || permanentDialog.visible || resultDialog.visible || helpDialog.visible || inspectorDialog.visible || issuesDialog.visible || errorDialog.visible || folderDialog.visible
    property string operationError: ""
    readonly property var selectedNode: appController.selected
    readonly property bool hasSelection: selectedNode && selectedNode.id !== undefined && selectedNode.id >= 0

    function bytes(value) {
        let amount = Number(value || 0)
        if (amount < 1024) return Math.round(amount) + " B"
        const units = ["KiB", "MiB", "GiB", "TiB", "PiB"]
        let index = -1
        do { amount /= 1024; index++ } while (amount >= 1024 && index < units.length - 1)
        return amount.toLocaleString(Qt.locale(), "f", amount >= 100 ? 0 : 1) + " " + units[index]
    }
    function count(value) { return Number(value || 0).toLocaleString(Qt.locale(), "f", 0) }
    function nodeSize(node) {
        if (!node) return "0 B"
        if (appController.metric === "files") return count(node.files) + " files"
        return bytes(appController.metric === "apparent" ? node.apparent : node.allocated)
    }
    function beginReview() {
        if (appController.busy || appController.queue.length === 0) return
        reviewData = appController.prepareReview()
        reviewDialog.open()
    }
    function openSelection() {
        if (hasSelection && selectedNode.directory) appController.navigate(appController.selectedId)
    }
    function markSelection() {
        if (hasSelection && !appController.busy) appController.toggleMark(appController.selectedId)
    }
    function categoryColor(category) {
        switch (category) {
        case "code": return theme.blue
        case "cache": return theme.yellow
        case "media": return theme.magenta
        case "scratch": case "agent": return theme.orange
        case "git": case "documents": return theme.magenta
        default: return theme.green
        }
    }

    component ActionButton: Button {
        id: control
        property bool primary: false
        property bool danger: false
        property bool quiet: false
        implicitHeight: 32
        implicitWidth: Math.max(32, contentItem.implicitWidth + 20)
        font.family: theme.fontFamily
        font.pixelSize: 12
        hoverEnabled: true
        padding: 8
        opacity: enabled ? 1 : 0.45
        contentItem: Text { textFormat: Text.PlainText;
            text: control.text
            font: control.font
            color: control.danger ? theme.red : theme.foreground
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        background: Rectangle {
            color: control.down ? theme.selection : control.hovered || control.primary ? theme.surface : "transparent"
            border.width: control.visualFocus ? 2 : 1
            border.color: control.visualFocus || control.primary ? theme.accent : control.danger ? theme.red : control.quiet && !control.hovered ? "transparent" : theme.border
            radius: 2
        }
        PlainToolTip { visible: control.hovered && control.Accessible.description.length > 0; text: control.Accessible.description; delay: 600 }
    }
    component PlainToolTip: ToolTip {
        id: tip
        padding: 8
        width: Math.min(window.width - 40, 600, contentItem.implicitWidth + 16)
        contentItem: Text {
            text: tip.text
            textFormat: Text.PlainText
            color: theme.foreground
            font.family: theme.fontFamily
            font.pixelSize: 12
            wrapMode: Text.WrapAnywhere
        }
        background: Rectangle { color: theme.surface; border.color: theme.border; radius: 2 }
    }
    component Caption: Label { textFormat: Text.PlainText;
        color: theme.muted
        font.pixelSize: 11
        elide: Text.ElideRight
    }
    component Rule: Rectangle {
        Layout.fillWidth: true
        implicitHeight: 1
        color: theme.border
    }
    component MetricRow: RowLayout {
        property string label
        property string value
        spacing: 12
        Caption { text: label; Layout.fillWidth: true }
        Label { textFormat: Text.PlainText; text: value; color: theme.foreground; font.pixelSize: 12 }
    }

    Shortcut { sequence: "Return"; enabled: !window.dialogActive && !search.activeFocus; onActivated: window.openSelection() }
    Shortcut { sequence: "Enter"; enabled: !window.dialogActive && !search.activeFocus; onActivated: window.openSelection() }
    Shortcut { sequence: "Backspace"; enabled: !window.dialogActive && !search.activeFocus; onActivated: appController.goUp() }
    Shortcut { sequence: "Space"; enabled: !window.dialogActive && !search.activeFocus; onActivated: window.markSelection() }
    Shortcut { sequence: "/"; enabled: !window.dialogActive && !search.activeFocus; onActivated: { window.listVisible = true; search.forceActiveFocus() } }
    Shortcut { sequence: "?"; enabled: !window.dialogActive && !search.activeFocus; onActivated: helpDialog.open() }
    Shortcut { sequence: "Ctrl+R"; enabled: !window.dialogActive && !appController.busy; onActivated: appController.rescan() }
    Shortcut { sequence: "Ctrl+L"; enabled: !window.dialogActive; onActivated: appController.chooseFolder() }
    Shortcut { sequence: "Ctrl+Return"; enabled: !window.dialogActive; onActivated: window.beginReview() }
    Shortcut { sequences: ["Down", "J"]; enabled: !window.dialogActive && !search.activeFocus; onActivated: appController.selectAdjacent(1) }
    Shortcut { sequences: ["Up", "K"]; enabled: !window.dialogActive && !search.activeFocus; onActivated: appController.selectAdjacent(-1) }
    Shortcut { sequences: ["Left", "H"]; enabled: !window.dialogActive && !search.activeFocus; onActivated: appController.goUp() }
    Shortcut { sequences: ["Right", "L"]; enabled: !window.dialogActive && !search.activeFocus; onActivated: window.openSelection() }
    Shortcut {
        sequence: "Escape"
        enabled: !window.dialogActive
        onActivated: {
            if (search.activeFocus || appController.filter.length) { appController.filter = ""; map.forceActiveFocus() }
            else if (appController.scanning) appController.cancelScan()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 55
            Layout.minimumHeight: 55
            Layout.maximumHeight: 55
            Layout.fillHeight: false
            Layout.leftMargin: 16
            Layout.rightMargin: 14
            spacing: 12
            Grid {
                columns: 2
                spacing: 2
                Repeater { model: [theme.blue, theme.cyan, theme.orange, theme.green]; Rectangle { required property color modelData; width: 7; height: 7; color: modelData } }
            }
            Label { textFormat: Text.PlainText; text: "blockyard"; font.pixelSize: 19; color: theme.foreground; visible: !window.compact }
            Rectangle { implicitWidth: 1; implicitHeight: 23; color: theme.border; visible: !window.compact }
            Flickable {
                id: crumbs
                Layout.fillWidth: true
                Layout.fillHeight: true
                contentWidth: crumbRow.width
                clip: true
                flickableDirection: Flickable.HorizontalFlick
                Row {
                    id: crumbRow
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 2
                    Repeater {
                        model: appController.breadcrumbs
                        Row {
                            required property var modelData
                            required property int index
                            Label { textFormat: Text.PlainText; text: " / "; color: theme.muted; anchors.verticalCenter: parent.verticalCenter; visible: index > 0 }
                            ActionButton { text: modelData.name || "/"; quiet: true; onClicked: appController.navigate(modelData.id); Accessible.description: modelData.path || "" }
                        }
                    }
                }
                onContentWidthChanged: contentX = Math.max(0, contentWidth - width)
            }
            ActionButton { text: "Choose folder"; enabled: !appController.busy; onClicked: appController.chooseFolder(); Accessible.description: "Choose a folder to scan · Ctrl+L" }
            ActionButton { text: appController.scanning ? "Stop scan" : "Rescan"; primary: true; enabled: !appController.busy; onClicked: appController.scanning ? appController.cancelScan() : appController.rescan(); Accessible.description: "Refresh the current scan · Ctrl+R" }
        }
        Rule {}
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 43
            Layout.minimumHeight: 43
            Layout.maximumHeight: 43
            Layout.fillHeight: false
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            spacing: 8
            Label { textFormat: Text.PlainText; text: appController.scanning ? "Scanning" : appController.metric === "files" ? window.count(appController.scanBytes) + " files" : window.bytes(appController.scanBytes); color: theme.foreground; font.pixelSize: 14 }
            Caption { text: appController.scanning ? window.count(appController.progressEntries) + " entries" : "in scan" }
            Caption { text: "/  " + window.count(appController.progressEntries) + " entries"; visible: window.width > 780 && !appController.scanning }
            Item { Layout.fillWidth: true }
            Repeater {
                model: [{label: "Code", key: "code"}, {label: "Scratch", key: "scratch"}, {label: "Media", key: "media"}, {label: "Cache", key: "cache"}]
                RowLayout {
                    required property var modelData
                    visible: window.width > 1080
                    spacing: 5
                    Rectangle { implicitWidth: 6; implicitHeight: 6; color: window.categoryColor(modelData.key) }
                    Caption { text: modelData.label }
                }
            }
            Caption { text: window.bytes(appController.availableBytes) + " available"; color: theme.foreground }
        }
        Rule {}
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 42
            Layout.minimumHeight: 42
            Layout.maximumHeight: 42
            Layout.fillHeight: false
            Layout.leftMargin: 10
            Layout.rightMargin: 10
            spacing: 5
            ActionButton { text: "↑"; enabled: appController.currentId >= 0; onClicked: appController.goUp(); Accessible.description: "Parent folder · Backspace" }
            ActionButton { text: window.listVisible ? "Hide list" : "Folders"; onClicked: window.listVisible = !window.listVisible; Accessible.description: "Show the folder list and search" }
            TextField {
                id: search
                objectName: "pathSearch"
                Layout.fillWidth: true
                Layout.minimumWidth: 70
                Layout.maximumWidth: 320
                implicitHeight: 30
                placeholderText: "Find a name  /"
                text: appController.filter
                placeholderTextColor: theme.muted
                color: theme.foreground
                selectionColor: theme.selection
                selectedTextColor: theme.foreground
                font.family: theme.fontFamily
                font.pixelSize: 12
                leftPadding: 9
                selectByMouse: true
                background: Rectangle { color: theme.background; radius: 2; border.color: search.activeFocus ? theme.accent : theme.border }
                onTextEdited: { appController.filter = text; if (text.length) window.listVisible = true }
                Keys.onEscapePressed: { appController.filter = ""; map.forceActiveFocus() }
                Keys.onDownPressed: { appController.selectAdjacent(1); folderList.forceActiveFocus() }
                Accessible.name: "Find a name in this scan"
            }
            Item { Layout.fillWidth: true }
            Repeater {
                model: [{label: "On disk", key: "allocated"}, {label: "File size", key: "apparent"}, {label: "Files", key: "files"}]
                ActionButton {
                    required property var modelData
                    text: modelData.label
                    visible: !window.compact
                    primary: appController.metric === modelData.key
                    quiet: !primary
                    onClicked: appController.metric = modelData.key
                    Accessible.description: modelData.key === "allocated" ? "Allocated bytes; shared blocks may be counted more than once" : modelData.key === "apparent" ? "Logical file length, including sparse regions" : "Number of files"
                }
            }
            ActionButton {
                text: (appController.metric === "allocated" ? "On disk" : appController.metric === "apparent" ? "File size" : "Files") + " ▾"
                visible: window.compact
                onClicked: metricMenu.popup()
                Menu {
                    id: metricMenu
                    background: Rectangle { color: theme.background; border.color: theme.border }
                    Repeater {
                        model: [{label: "On disk", key: "allocated"}, {label: "File size", key: "apparent"}, {label: "File count", key: "files"}]
                        MenuItem {
                            required property var modelData
                            text: modelData.label
                            onTriggered: appController.metric = modelData.key
                            contentItem: Label { textFormat: Text.PlainText; text: parent.text; color: theme.foreground; padding: 8 }
                            background: Rectangle { color: parent.highlighted ? theme.selection : theme.background }
                        }
                    }
                }
            }
            Caption { text: "Depth " + appController.depth; visible: window.width > 740 }
            ActionButton { text: "−"; visible: window.width > 740; enabled: appController.depth > 1; onClicked: appController.depth--; Accessible.description: "Show fewer nested levels" }
            ActionButton { text: "+"; visible: window.width > 740; enabled: appController.depth < 6; onClicked: appController.depth++; Accessible.description: "Show more nested levels" }
            ActionButton { text: "Details"; visible: window.compact; onClicked: inspectorDialog.open() }
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0
            Rectangle {
                visible: window.listVisible
                Layout.preferredWidth: window.compact ? 210 : 250
                Layout.fillHeight: true
                color: theme.background
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.margins: 10
                        Caption { text: appController.filter.length ? "Matching names" : "Largest first"; Layout.fillWidth: true }
                        Caption { text: appController.metric === "files" ? "Files" : "Size" }
                    }
                    ListView {
                        id: folderList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: appController.rows
                        boundsBehavior: Flickable.StopAtBounds
                        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                        delegate: ItemDelegate {
                            id: fileRow
                            required property var modelData
                            width: ListView.view.width
                            height: appController.filter.length ? 52 : 36
                            padding: 8
                            background: Rectangle { color: fileRow.modelData.id === appController.selectedId ? theme.selection : fileRow.hovered ? theme.surface : "transparent"; border.color: fileRow.visualFocus ? theme.accent : "transparent" }
                            contentItem: RowLayout {
                                spacing: 7
                                Label { textFormat: Text.PlainText; text: fileRow.modelData.directory ? "▸" : "·"; color: theme.muted }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 0
                                    Label { textFormat: Text.PlainText; text: fileRow.modelData.name; Layout.fillWidth: true; elide: Text.ElideRight; color: theme.foreground; font.pixelSize: 12 }
                                    Caption { text: fileRow.modelData.path; Layout.fillWidth: true; visible: appController.filter.length > 0; elide: Text.ElideMiddle }
                                }
                                Caption { text: window.nodeSize(fileRow.modelData); color: theme.foreground }
                                Label { textFormat: Text.PlainText; text: appController.markedIds.indexOf(fileRow.modelData.id) >= 0 ? "✓" : ""; color: theme.accent; Layout.preferredWidth: 12 }
                            }
                            onClicked: appController.selectNode(modelData.id)
                            onDoubleClicked: { if (modelData.directory) appController.navigate(modelData.id) }
                            PlainToolTip { visible: fileRow.hovered; text: fileRow.modelData.path; delay: 800 }
                            Accessible.name: modelData.name + ", " + window.nodeSize(modelData)
                        }
                        Label { textFormat: Text.PlainText; anchors.centerIn: parent; visible: folderList.count === 0; text: appController.filter.length ? "No matching names" : "No entries yet"; color: theme.muted; font.pixelSize: 12 }
                        footer: ActionButton {
                            width: folderList.width
                            text: "Show more (" + folderList.count + " of " + appController.rowsTotal + ")"
                            visible: appController.rowsTotal > folderList.count
                            height: visible ? 34 : 0
                            onClicked: appController.loadMoreRows()
                        }
                    }
                }
            }
            Rectangle { visible: window.listVisible; Layout.fillHeight: true; implicitWidth: 1; color: theme.border }
            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.margins: 7
                Treemap {
                    id: map
                    objectName: "storageMap"
                    anchors.fill: parent
                    model: appController.mapCells
                    selectedId: appController.selectedId
                    markedIds: appController.markedIds
                    fontFamily: window.colors.fontFamily
                    theme: window.colors
                    focus: true
                    onSelected: function(id) { appController.selectNode(id); forceActiveFocus() }
                    onOpened: function(id) { appController.navigate(id) }
                    onMarked: function(id) { appController.toggleMark(id) }
                    Accessible.name: "Storage treemap"
                    Accessible.description: "Select a rectangle to inspect it. Double-click a folder to open it. Use Folders for a text list."
                    PlainToolTip { visible: map.hoveredPath.length > 0; text: map.hoveredPath; delay: 700 }
                }
                ColumnLayout {
                    anchors.centerIn: parent
                    width: Math.min(parent.width - 32, 350)
                    visible: appController.mapCells.length === 0
                    spacing: 12
                    Label { textFormat: Text.PlainText; text: appController.scanning ? "Reading folders" : appController.scanStatus === "failed" ? "Scan could not complete" : appController.currentId < 0 ? "Nothing to map yet" : appController.issueCount > 0 && appController.rowsTotal === 0 ? "No readable entries" : appController.rowsTotal === 0 && !appController.filter.length ? "This folder is empty" : "No area in this view"; color: theme.foreground; font.pixelSize: 23; Layout.alignment: Qt.AlignHCenter }
                    Label { textFormat: Text.PlainText; text: appController.scanning ? "The map will appear when the scan finishes. Stop the scan to explore the folders counted so far." : appController.scanStatus === "failed" ? appController.scanMessage : appController.currentId < 0 ? "Choose a folder to see what occupies its space." : appController.issueCount > 0 && appController.rowsTotal === 0 ? "The scan could not fully read this folder. Check the scan issues before drawing conclusions about its size." : appController.rowsTotal === 0 && !appController.filter.length ? "Go up to explore another folder." : "Zero-byte items and empty folders remain available in the list. File size can show sparse files with no allocated blocks."; color: theme.muted; wrapMode: Text.WordWrap; horizontalAlignment: Text.AlignHCenter; Layout.fillWidth: true }
                    ActionButton { text: appController.currentId < 0 ? "Choose folder" : appController.rowsTotal > 0 ? "Show folder list" : "Go up"; visible: !appController.scanning; Layout.alignment: Qt.AlignHCenter; onClicked: appController.currentId < 0 ? appController.chooseFolder() : appController.rowsTotal > 0 ? window.listVisible = true : appController.goUp() }
                }
            }
            Rectangle {
                visible: !window.compact
                Layout.preferredWidth: 276
                Layout.fillHeight: true
                color: theme.surface
                Rectangle { anchors.left: parent.left; height: parent.height; width: 1; color: theme.border }
                ScrollView {
                    id: desktopInspectorScroll
                    anchors.fill: parent
                    anchors.margins: 17
                    clip: true
                    Inspector { width: desktopInspectorScroll.availableWidth; height: Math.max(550, desktopInspectorScroll.availableHeight) }
                }
            }
        }
        Rule {}
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 45
            Layout.minimumHeight: 45
            Layout.maximumHeight: 45
            Layout.fillHeight: false
            Layout.leftMargin: 14
            Layout.rightMargin: 12
            spacing: 10
            Caption { text: "Review queue" }
            Label { textFormat: Text.PlainText; text: appController.queue.length ? appController.queue.length + (appController.queue.length === 1 ? " item  /  " : " items  /  ") + window.bytes(appController.queueBytes) : "Mark files or folders with Space"; color: appController.queue.length ? theme.foreground : theme.muted; Layout.fillWidth: true; elide: Text.ElideRight; font.pixelSize: 12 }
            ActionButton { text: "Clear"; quiet: true; visible: appController.queue.length > 0; enabled: !appController.busy; onClicked: appController.clearQueue() }
            ActionButton { objectName: "reviewButton"; text: "Review"; primary: true; enabled: appController.queue.length > 0 && !appController.busy; onClicked: window.beginReview(); Accessible.description: "Review marked paths before removal · Ctrl+Enter" }
        }
        Rule {}
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 31
            Layout.minimumHeight: 31
            Layout.maximumHeight: 31
            Layout.fillHeight: false
            Layout.leftMargin: 12
            Layout.rightMargin: 12
            spacing: 9
            ActionButton { text: "? Keys"; quiet: true; implicitHeight: 25; onClicked: helpDialog.open() }
            Caption { text: "Enter open   ⌫ up   Space mark   / find"; visible: window.width > 850 }
            Item { Layout.fillWidth: true }
            Caption { text: appController.busy ? "Applying reviewed action…" : appController.scanning ? "Scanning · " + window.count(appController.progressEntries) + " entries · " + (appController.scanElapsed / 1000).toFixed(1) + "s" : appController.scanMessage || (appController.scanStatus === "complete" ? "Scan complete" : ""); Layout.maximumWidth: window.width * 0.45 }
            ActionButton { text: appController.issueCount + " skipped"; visible: appController.issueCount > 0; quiet: true; implicitHeight: 25; onClicked: issuesDialog.open(); Accessible.description: "Read scan issues. These sizes may be incomplete." }
        }
    }

    component Inspector: ColumnLayout {
        spacing: 9
        Caption { text: window.hasSelection ? "Selected " + (window.selectedNode.directory ? "folder" : "file") : "Selection" }
        Label { textFormat: Text.PlainText; text: window.hasSelection ? window.selectedNode.name || "/" : "Select a block"; color: theme.foreground; font.pixelSize: 22; Layout.fillWidth: true; elide: Text.ElideMiddle }
        Label { textFormat: Text.PlainText; text: window.hasSelection ? window.selectedNode.path : "Explore the map or open the folder list."; color: theme.muted; font.pixelSize: 11; wrapMode: Text.WrapAnywhere; Layout.fillWidth: true; maximumLineCount: 3; elide: Text.ElideMiddle }
        Label { textFormat: Text.PlainText; text: window.hasSelection ? window.nodeSize(window.selectedNode) : ""; color: theme.foreground; font.pixelSize: 31; Layout.topMargin: 7; Layout.bottomMargin: 5 }
        MetricRow { label: "Files"; value: window.count(window.selectedNode.files); visible: window.hasSelection }
        MetricRow { label: "File size"; value: window.bytes(window.selectedNode.apparent); visible: window.hasSelection && appController.metric !== "apparent" }
        MetricRow { label: "On disk"; value: window.bytes(window.selectedNode.allocated); visible: window.hasSelection && appController.metric !== "allocated" }
        MetricRow { label: "Kind"; value: window.selectedNode.symlink ? "Symbolic link" : window.selectedNode.category || (window.selectedNode.directory ? "Folder" : "File"); visible: window.hasSelection }
        MetricRow { label: "Modified"; value: Qt.formatDateTime(new Date(Number(window.selectedNode.modified || 0) * 1000), "yyyy-MM-dd HH:mm"); visible: window.hasSelection && Number(window.selectedNode.modified) > 0 }
        Label { textFormat: Text.PlainText; text: window.selectedNode.error || window.selectedNode.protectedReason || ""; visible: text.length > 0; color: theme.orange; wrapMode: Text.WordWrap; Layout.fillWidth: true; font.pixelSize: 11 }
        Rule { Layout.topMargin: 7; Layout.bottomMargin: 5 }
        ActionButton {
            text: appController.markedIds.indexOf(appController.selectedId) >= 0 ? "Remove from review" : "Add to review   Space"
            primary: true
            Layout.fillWidth: true
            enabled: window.hasSelection && !appController.busy && !window.selectedNode.protectedReason
            onClicked: window.markSelection()
        }
        RowLayout {
            Layout.fillWidth: true
            ActionButton { text: "Open folder"; quiet: true; Layout.fillWidth: true; enabled: window.hasSelection; onClicked: appController.openSelected() }
            ActionButton { text: "Copy path"; quiet: true; enabled: window.hasSelection; onClicked: appController.copySelectedPath() }
        }
        Label { textFormat: Text.PlainText;
            text: window.selectedNode.category === "cache" ? "Cache contents may be recreated. Inspect them before removing anything." : window.selectedNode.category === "scratch" || window.selectedNode.category === "agent" ? "Check worktrees and experiment folders for work you want to keep." : "Selected size does not predict space freed. Shared blocks and snapshots can retain data."
            color: theme.muted
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
            font.pixelSize: 11
            lineHeight: 1.3
            visible: window.hasSelection
        }
        Item { Layout.fillHeight: true; Layout.minimumHeight: 10 }
        Rule {}
        MetricRow { label: "Disk available"; value: window.bytes(appController.availableBytes) }
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 5
            color: theme.border
            Rectangle { width: parent.width * Math.max(0, Math.min(1, appController.totalBytes ? 1 - appController.availableBytes / appController.totalBytes : 0)); height: parent.height; color: theme.muted }
        }
        Caption { text: window.bytes(appController.totalBytes) + " total capacity" }
        ActionButton { text: "Open system Trash"; quiet: true; Layout.fillWidth: true; onClicked: appController.openTrash() }
    }

    component AppDialog: Dialog {
        id: dialogControl
        parent: Controls.Overlay.overlay
        anchors.centerIn: parent
        modal: true
        padding: 22
        width: Math.min(window.width - 40, 640)
        closePolicy: Popup.CloseOnEscape
        palette.windowText: theme.foreground
        palette.text: theme.foreground
        background: Rectangle { color: theme.background; border.color: theme.border; border.width: 1; radius: 3 }
        header: Label { textFormat: Text.PlainText; text: dialogControl.title; color: theme.foreground; font.pixelSize: 21; padding: 22; bottomPadding: 2; elide: Text.ElideRight }
        Controls.Overlay.modal: Rectangle { color: Qt.rgba(theme.background.r, theme.background.g, theme.background.b, 0.75) }
    }
    AppDialog {
        id: reviewDialog
        objectName: "reviewDialog"
        title: "Review removal"
        contentItem: ColumnLayout {
            spacing: 15
            Label { textFormat: Text.PlainText; text: window.reviewData.items.length + " items  /  " + window.bytes(window.reviewData.total) + " selected"; color: theme.foreground }
            Label { textFormat: Text.PlainText; text: "Check every path. Selected bytes are not a promise of space freed."; color: theme.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            ListView {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(210, contentHeight)
                clip: true
                model: window.reviewData.items
                ScrollBar.vertical: ScrollBar {}
                delegate: Column {
                    required property var modelData
                    width: ListView.view.width
                    spacing: 3
                    Label { textFormat: Text.PlainText; text: modelData.path || ""; color: theme.foreground; font.pixelSize: 12; width: parent.width; wrapMode: Text.WrapAnywhere }
                    Caption { text: window.bytes(modelData.allocated !== undefined ? modelData.allocated : modelData.bytes) }
                    Item { width: 1; height: 9 }
                }
            }
            Label { textFormat: Text.PlainText; text: window.reviewData.error || ""; visible: text.length > 0; color: theme.red; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            Label { textFormat: Text.PlainText; text: (window.reviewData.warnings || []).join("\n"); visible: text.length > 0; color: theme.orange; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            Rule {}
            Label { textFormat: Text.PlainText; text: "Trash keeps files recoverable and usually keeps their space occupied. Permanent removal cannot be undone by Blockyard."; color: theme.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true; font.pixelSize: 12 }
            RowLayout {
                Layout.fillWidth: true
                ActionButton { text: "Cancel"; onClicked: reviewDialog.close() }
                Item { Layout.fillWidth: true }
                ActionButton { objectName: "permanentChoice"; text: "Permanent removal…"; danger: true; enabled: !window.reviewData.error && window.reviewData.items.length > 0 && !appController.busy; onClicked: { reviewDialog.close(); permanentDialog.open() } }
                ActionButton { objectName: "moveToTrash"; text: "Move to Trash"; primary: true; enabled: !window.reviewData.error && window.reviewData.items.length > 0 && !appController.busy; onClicked: { reviewDialog.close(); appController.executeReview("trash") } }
            }
        }
    }
    AppDialog {
        id: permanentDialog
        objectName: "permanentDialog"
        title: "Permanently remove these items?"
        contentItem: ColumnLayout {
            spacing: 16
            Label { textFormat: Text.PlainText; text: "These paths will be permanently removed. Blockyard cannot restore them."; color: theme.foreground; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            ScrollView {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(180, pathsText.implicitHeight + 10)
                TextArea { textFormat: TextEdit.PlainText; id: pathsText; text: window.reviewData.items.map(item => item.path).join("\n\n"); readOnly: true; wrapMode: Text.WrapAnywhere; color: theme.foreground; font.pixelSize: 12; selectByMouse: true; background: null }
            }
            Label { textFormat: Text.PlainText; text: window.bytes(window.reviewData.total) + " selected. Shared blocks or snapshots may keep space occupied."; color: theme.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            RowLayout {
                Layout.fillWidth: true
                ActionButton { text: "Cancel"; onClicked: permanentDialog.close() }
                Item { Layout.fillWidth: true }
                ActionButton { objectName: "confirmPermanent"; text: "Permanently remove"; danger: true; enabled: !appController.busy; onClicked: { permanentDialog.close(); appController.executeReview("permanent") } }
            }
        }
    }
    AppDialog {
        id: resultDialog
        objectName: "resultDialog"
        title: "Removal results"
        contentItem: ColumnLayout {
            spacing: 14
            Label { textFormat: Text.PlainText; text: "Each path reports its own result. Files moved to Trash still occupy space."; color: theme.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            Label { textFormat: Text.PlainText; text: appController.lastCleanupSpaceKnown ? "Observed available-space change: " + (appController.lastCleanupSpaceDelta >= 0 ? "+" : "−") + window.bytes(Math.abs(appController.lastCleanupSpaceDelta)) + ". Other running apps can also affect this number." : "Available-space change could not be measured."; color: theme.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true; font.pixelSize: 12 }
            ListView {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(300, contentHeight)
                clip: true
                model: window.cleanupOutcomes
                ScrollBar.vertical: ScrollBar {}
                delegate: Column {
                    required property var modelData
                    width: ListView.view.width
                    spacing: 4
                    Label { textFormat: Text.PlainText; text: modelData.path || ""; width: parent.width; wrapMode: Text.WrapAnywhere; color: theme.foreground; font.pixelSize: 12 }
                    Label { textFormat: Text.PlainText; text: modelData.message || modelData.error || modelData.status || "Completed"; width: parent.width; wrapMode: Text.WordWrap; color: modelData.success === false || modelData.error ? theme.red : theme.muted; font.pixelSize: 12 }
                    Item { height: 10; width: 1 }
                }
            }
            RowLayout {
                Layout.fillWidth: true
                ActionButton { text: "Open Trash"; onClicked: appController.openTrash() }
                Item { Layout.fillWidth: true }
                ActionButton { text: "Done"; primary: true; onClicked: resultDialog.close() }
            }
        }
    }
    AppDialog {
        id: inspectorDialog
        title: "Details"
        width: Math.min(390, window.width - 40)
        height: Math.min(650, window.height - 40)
        contentItem: ScrollView { id: inspectorScroll; clip: true; Inspector { width: inspectorScroll.availableWidth; height: Math.max(535, inspectorScroll.availableHeight) } }
    }
    AppDialog {
        id: helpDialog
        title: "Keyboard controls"
        contentItem: ColumnLayout {
            spacing: 13
            Repeater {
                model: [{key: "↑ ↓  or  j k", action: "Select previous / next item"}, {key: "Enter  or  l", action: "Open selected folder"}, {key: "Backspace  or  h", action: "Go to parent folder"}, {key: "Space", action: "Add or remove from review"}, {key: "/", action: "Find a name"}, {key: "Ctrl+Enter", action: "Review marked items"}, {key: "Ctrl+R", action: "Rescan"}, {key: "Ctrl+L", action: "Choose a folder"}, {key: "Escape", action: "Clear search / stop scan / close dialog"}]
                RowLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    Label { textFormat: Text.PlainText; text: modelData.key; color: theme.accent; Layout.preferredWidth: 170 }
                    Label { textFormat: Text.PlainText; text: modelData.action; color: theme.foreground; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                }
            }
            ActionButton { text: "Close"; Layout.alignment: Qt.AlignRight; onClicked: helpDialog.close() }
        }
    }
    AppDialog {
        id: issuesDialog
        title: "Scan issues"
        contentItem: ColumnLayout {
            spacing: 14
            Label { textFormat: Text.PlainText; text: "Some paths could not be fully scanned. Their sizes may be incomplete."; color: theme.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            ScrollView {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(300, window.height * 0.45)
                TextArea { textFormat: TextEdit.PlainText; text: appController.issues.join("\n\n"); readOnly: true; wrapMode: Text.WrapAnywhere; color: theme.foreground; font.pixelSize: 12; selectByMouse: true; background: null }
            }
            ActionButton { text: "Close"; Layout.alignment: Qt.AlignRight; onClicked: issuesDialog.close() }
        }
    }
    Connections {
        target: appController
        function onCleanupFinished(outcomes) { window.cleanupOutcomes = outcomes; resultDialog.open() }
        function onFolderRequested() { folderDialog.open() }
        function onErrorOccurred(message) { window.operationError = message; errorDialog.open() }
    }
    NativeDialogs.FolderDialog {
        id: folderDialog
        title: "Choose a folder to scan"
        onAccepted: appController.startScanUrl(selectedFolder)
    }
    AppDialog {
        id: errorDialog
        title: "Action could not complete"
        contentItem: ColumnLayout {
            spacing: 16
            Label { textFormat: Text.PlainText; text: window.operationError; color: theme.foreground; wrapMode: Text.WrapAnywhere; Layout.fillWidth: true }
            ActionButton { text: "Close"; Layout.alignment: Qt.AlignRight; onClicked: errorDialog.close() }
        }
    }
}
