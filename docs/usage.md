# Using Blockyard

Open **Blockyard** from the application launcher or run `blockyard` to scan your home folder. Pass a folder to start there instead:

```sh
blockyard /path/to/folder
```

Use **Choose folder** to scan another location. The scan is a point-in-time observation. Canceled scans remain available to explore, but cleanup requires a complete scan. Use **Rescan** to refresh changes made by other apps.

## Navigation

Click a block to inspect it. Double-click a folder to enter it. The Folders button opens a precise, size-sorted list, including entries too small to label on the map. Search covers names across the current scan.

| Key | Action |
| --- | --- |
| Enter / L | Open selected folder |
| Backspace / H | Parent folder |
| Up / Down / J / K | Select entries in size order |
| Space | Add or remove the selected item from review |
| / | Search |
| Ctrl+Enter | Review marked items |
| Ctrl+R | Rescan |
| Ctrl+L | Choose folder |
| ? | Keyboard help |
| Escape | Clear search, close a dialog, or stop scanning |

The inspector moves into a Details dialog when the window is narrow. There are no continuous animations or polling scans.

## Cleanup and size accounting

**Selected bytes are not a promise of freed space.** Btrfs compression, reflinks and snapshots, hard links and files held open by running programs can make folder totals differ from disk usage. Blockyard shows allocated size, logical file size and filesystem capacity separately. Cleanup reports the observed change in available space, which other running applications may also affect.

Trash is the default action. It preserves files and does not usually free space. Restore or empty those files using your file manager's system Trash. Blockyard writes standard freedesktop Trash metadata with the original path. It supports the home Trash on the same filesystem; if that is unavailable, it fails without substituting permanent removal.

Permanent removal has a separate confirmation and cannot be undone by Blockyard. The engine rechecks selected objects and stages them into a private directory on the same filesystem before acting. If an item changes, the operation stops for that item and preserves what remains. Partial failures identify recovery locations.

The scan root, mounted descendants, nested Btrfs subvolumes, incomplete scans, linked Git worktrees and recognized service/configuration data are protected. Scan other mounts separately. Blockyard does not run elevated, prune package stores, manage snapshots, or infer that an old folder is disposable. See [engine behavior and limits](engine.md).

## Omarchy integration

Blockyard observes the active theme directory and repairs its watches when Omarchy replaces it. It reads colors as data and uses fontconfig for the active monospace font. Contrast adjustments keep labels readable. Temporary partial writes retain the last valid palette. See [theme compatibility](theming.md).

A `--theme-home DIRECTORY` option lets tests load copied theme fixtures without altering the desktop. `--screenshot FILE` saves the app window after a scan and exits. Neither option mutates scanned files.

## Installation and help

[Download the Arch package](https://github.com/allisonmahmood/blockyard/releases/latest), [build from source](development.md), or [report an issue](https://github.com/allisonmahmood/blockyard/issues).

[Back to Blockyard](../README.md)
