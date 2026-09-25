# Storage engine

The engine is a QtCore and C++20 library. It keeps native Linux filename bytes and authoritative integer sizes outside the QML interface. Its public contract is in `src/engine/engine.h`.

`displayPath` is the shared display formatter for paths and names. Printable UTF-8 stays readable; control characters, backslashes, Unicode direction controls, and invalid filename bytes become explicit escapes. The result must never become an operating-system path. Actions retain node IDs and original bytes.

## Scanner

`scan` returns an immutable tree after completion or cancellation. A cancelled tree remains usable for inspection, but cleanup requires a completed scan. Progress reports the entries found and elapsed milliseconds. It does not estimate a percentage before the total is known.

The scanner uses `readdir`, `fstatat`, and descriptor-relative `openat2`. It includes hidden files, never follows descendant symlinks, and reads metadata rather than file contents. Entries in each directory are sorted by native name for repeatable hard-link attribution. A node stores its parent and basename; it does not store a full path. Directory traversal holds a bounded number of descriptors. The current implementation is serial; parallel workers should be justified by a measured bottleneck.

Current upstream gdu has a useful scanner and web interface, but adapting its Go engine would add a process or a language bridge to the approved native Qt application. A small C++ scanner avoids that dependency and keeps native identities available to the cleanup code. No gdu code was copied. [gdu](https://github.com/dundee/gdu)

The scan stays within the selected mount. Nested mounts, including bind mounts, appear as protected entries with no inferred contents. Nested Btrfs subvolumes are also boundaries and can be scanned separately. This avoids treating snapshot inode numbers as unique across the filesystem. [Linux openat2](https://man7.org/linux/man-pages/man2/openat2.2.html), [Btrfs inode numbers](https://btrfs.readthedocs.io/en/latest/Subvolumes.html#inode-numbers)

`allocated` sums `st_blocks * 512`, and `apparent` sums file lengths and directory metadata lengths. Within a scan, hard-linked regular files contribute bytes once; all directory entries remain browsable and count toward the item total. A repeated hard-link entry therefore has zero attributed map bytes even though its native identity records the real length. Sparse files can have a large apparent size and a small allocated size. [Linux stat](https://man7.org/linux/man-pages/man2/stat.2.html)

Map totals are not disk capacity or guaranteed reclaimable space. Btrfs reflinks and snapshots share extents; open files can retain blocks after their final pathname disappears. `diskSpace` separately returns available capacity for the current user and filesystem capacity. The controller must not add capacities from separate mounts of the same storage pool. [Btrfs usage accounting](https://btrfs.readthedocs.io/en/latest/btrfs-filesystem.html), [Linux unlink](https://man7.org/linux/man-pages/man2/unlink.2.html)

Errors remain on their nodes and propagate incomplete status to parents. A missing or unreadable subtree is not silently reported as empty. The tree stores a bounded list of example errors and a total issue count. The view/controller owns pagination and the visible rectangle limit.

## Cleanup

`prepareCleanup` validates IDs, refuses protected or incomplete items, and removes redundant descendant selections. The returned object holds the exact immutable scan used for review. The controller must discard it whenever the review changes or another scan replaces the tree.

Execution checks the scan root and each ancestor by device, inode, and object type, opens them without following symlinks or crossing mounts, and rechecks linked-worktree markers. It then validates the selected subtree against the scan, including names, identities, size, mode, and modification timestamps. Newly added children and replaced files invalidate the review. A batch remembers hard-link changes caused by its own completed operations so removing one reviewed link does not invalidate another reviewed link.

Each selected item moves atomically into a new private directory in its own parent before any irreversible action. The engine verifies it again there. Permanent removal detaches each child into that private directory and verifies the detached identity before unlinking. A process holding the old parent directory open cannot exchange that detached name. Symlinks themselves can be removed; their targets are not traversed. [Linux renameat2](https://man7.org/linux/man-pages/man2/rename.2.html), [Linux unlinkat](https://man7.org/linux/man-pages/man2/unlink.2.html)

This handles ordinary concurrent writers and path substitution. It is not a sandbox against a hostile process with the same user privileges that discovers and deliberately edits the private staging directory. Existing open handles can still change the contents of an approved inode. Recursive cleanup is not a filesystem transaction. If an operation fails or is cancelled, previously removed files stay removed, and the remaining selection is restored when its original name is free. Recovery never overwrites a replacement. Otherwise, the result names the private `.blockyard-cleanup-*` directory containing the remaining data. A future scan marks recovery directories as protected.

The engine refuses the scan root, system-managed paths, selected service directories, credentials, configuration, Git metadata, linked worktrees, mount boundaries, and directories containing protected descendants. These are conservative inspection boundaries, not proof that every other folder is disposable. Category labels only help navigation.

## Trash

Trash uses the freedesktop home-trash format directly. It creates an exclusive, unpredictable name, writes the original absolute native path using percent encoding and the local deletion date to `.trashinfo`, flushes the metadata, and then renames the staged item into `Trash/files`. Directories must be owned by the user, private, and opened without symlink traversal. A failed move removes only the metadata created for that attempted move, then restores the staged selection. [Freedesktop Trash specification](https://specifications.freedesktop.org/trash/latest/)

This adapter deliberately supports only `$XDG_DATA_HOME/Trash`, or `$HOME/.local/share/Trash` when XDG data is unset, on the same filesystem as the selected item. The XDG data directory must already exist. It does not copy across filesystems, fall back to permanent deletion, or create per-volume trash directories. An unsupported location produces a clear error. The user can explicitly choose permanent removal instead.

Trash does not free the data. The operating system's file manager handles restore and emptying; the application has no restore database. Tests verify metadata for newline, percent, and invalid UTF-8 filename bytes, and verify system `gio` restore in an isolated D-Bus session and temporary XDG home.

## Tests and limits

`engine_test` covers hidden and native names, sparse files, hard-link accounting and batch removal, symlinks, cancellation, protected descendants, stale selections, ancestor swaps, metadata changes, permission errors, staging cleanup, and Trash failures. All mutation fixtures use temporary directories. `adversarial_test` independently exercises additional identity, worktree, and controller-review attacks.

Linux `openat2` and `renameat2` are required. Unsupported kernels fail closed. A large directory must be enumerated and sorted before its children are added, though enumeration checks cancellation. No persistent database, watcher tree, background daemon, duplicate hashing, snapshot deletion, or package-manager cleanup is included.
