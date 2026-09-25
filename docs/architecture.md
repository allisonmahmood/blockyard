# Architecture

Blockyard is a native Qt 6 application. The project explicitly excludes Tauri and browser-based desktop shells.

## Modules

`src/engine` owns the scan tree, native filenames, file identity, size aggregation, mount boundaries and cleanup. A cancellable worker scans metadata without reading file contents. The tree uses parent indices and relative names instead of storing a full path for each entry.

`src/controller` runs scans and cleanup off the GUI thread. It owns navigation, selection, a normalized review queue and one-use prepared cleanup state. It supplies bounded visible data to QML. New scans clear old selections and review state.

`src/treemap` is a QQuickPaintedItem with a squarified layout, nested directory headers, hit testing and selection. It draws only when data, geometry, selection, hover or theme changes. It sends node IDs back to the controller. Subpixel or omitted entries remain reachable in the folder list.

`qml/Main.qml` contains Atlas, ordinary Qt Quick controls, keyboard actions and explicit review dialogs. It never executes a shell command or performs filesystem mutations itself.

`src/desktop/theme` observes Omarchy palette/font changes. It owns contrast-derived colors and watcher repair. It runs no hooks and writes no configuration.

## Choices

A direct native scanner keeps this app to one process and one language for filesystem operations. Gdu was considered because its current web interface and scanner already cover much of the problem. Embedding its Go backend would add a process boundary or require replacing the approved native Qt design. The initial implementation therefore uses Linux metadata APIs with a simple serial traversal, then measures the result before adding worker-pool complexity.

Qt's retained window scene and a bounded painted map are sufficient for this interface. The application starts with no scan database, recursive filesystem watcher, background daemon, cache-cleaner plugins or index format. A completed scan remains in memory until a rescan or exit.

The map is bounded to about 1,500 visible nodes and 160 siblings per branch; an explicit remaining-items tile represents omitted mass. The list is paged in batches of 200. Scan data stays native until requested by the interface. UI byte values use doubles for display and geometry; authoritative cleanup accounting remains unsigned 64-bit in the engine.

## Scan and cleanup lifecycle

Only one scan or cleanup runs at a time. A request for a new root cancels the current scan and starts after its worker finishes. Cancellation retains an explicitly incomplete tree, and cleanup rejects it. Progress reports entries observed and elapsed time, not a fabricated percentage.

Marking a parent subsumes marked descendants. Review resolves IDs against the held tree. Changing the queue or starting a scan invalidates the prepared action. The engine revalidates actual filesystem identities again at execution.

Trash and permanent removal are separate actions. The home Trash adapter deliberately supports only a safely opened, privately owned Trash on the same filesystem. Broader mount-specific Trash support can be added when it has tests proving equivalent guarantees.

## Test boundaries

Engine tests use temporary files, symlinks, sparse files and native names. Theme tests copy fixtures and replace directories or symlinks; they never change the user's selected theme. GUI tests load the real QML and renderer against a deterministic controller fixture, while controller tests exercise real asynchronous scanning and cleanup against disposable trees. Independent adversarial tests challenge the mutation boundary.
