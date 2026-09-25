import QtQuick
QtObject {
    property string rootPath: "/fixture"
    property string currentPath: "/fixture"
    property string scanStatus: "complete"
    property string scanMessage: "Scan complete"
    property bool scanning: false
    property bool busy: false
    property int progressEntries: 3
    property int issueCount: 0
    property int selectedId: 1
    property int currentId: 0
    property int depth: 3
    property double scanElapsed: 0.1
    property double totalBytes: 1073741824
    property double availableBytes: 536870912
    property double scanBytes: 4096
    property var issues: []
    property var selected: ({id: 1, parentId: 0, name: "Disposable", path: "/fixture/Disposable", directory: true, symlink: false, allocated: 4096, apparent: 4096, files: 1, modified: "", category: "code", error: "", protectedReason: "", childCount: 1})
    property var breadcrumbs: [{id: 0, name: "fixture", path: "/fixture"}]
    property var rows: [selected]
    property var queue: []
    property double queueBytes: queue.length ? 4096 : 0
    property string metric: "allocated"
    property string filter: ""
    property var markedIds: queue.length ? [1] : []
    property var mapCells: [{id: 1, name: "Disposable", path: "/fixture/Disposable", category: "code", value: 4096, metric: "allocated", children: []}]
    property double lastCleanupSpaceDelta: 0
    property bool lastCleanupSpaceKnown: true
    property int rowsTotal: 1
    property int mutationCalls: 0
    property string lastAction: ""
    signal cleanupFinished(var outcomes)
    signal errorOccurred(string message)
    signal folderRequested()
    function startScan(path) {}
    function startScanUrl(url) {}
    function cancelScan() { scanning = false }
    function rescan() {}
    function navigate(id) { currentId = id }
    function selectNode(id) { selectedId = id }
    function goUp() { currentId = 0 }
    function selectAdjacent(delta) {}
    function toggleMark(id) { queue = queue.length ? [] : [selected] }
    function clearQueue() { queue = [] }
    function prepareReview() { return {items: queue, total: queueBytes, error: "", warnings: []} }
    function executeReview(action) { mutationCalls++; lastAction = action }
    function openSelected() {}
    function openTrash() {}
    function copySelectedPath() {}
    function chooseFolder() { folderRequested() }
    function loadMoreRows() {}
    function useHostileName() { selected = Object.assign({}, selected, {name: "<b>literal</b>", path: "/fixture/<b>literal</b>"}); queue = [selected] }
}
