# Blockyard

A native disk-space explorer for Omarchy. See where storage goes, drill into a treemap, and review selected files before moving them to Trash or permanently removing them.

Built with Qt 6 / Qt Quick and C++20. No browser renderer, background daemon or account.

## What it does

- Shows a nested treemap, a searchable folder list and details for the selected item.
- Scans hidden files, counts hard-linked data once, distinguishes file length from allocated blocks, and reports unreadable or excluded areas.
- Follows Omarchy's active theme and font while running. Supports current named palettes, older ANSI palettes and Alacritty theme files, including light themes.
- Supports keyboard navigation, a review queue, Trash and explicit permanent removal.
- Opens folders and the system Trash in your file manager.

A scan is a point-in-time observation. Scan results stay usable after cancellation, but cleanup requires a complete scan. Use Rescan to refresh changes made by other applications.

## Download for Omarchy / Arch

Get the x86_64 Arch package from [GitHub Releases](https://github.com/allisonmahmood/blockyard/releases/latest). Download the `.pkg.tar.zst` file and `SHA256SUMS`, then run in that directory:

```sh
sha256sum --ignore-missing --check SHA256SUMS
sudo pacman -U ./blockyard-0.1.0-1-x86_64.pkg.tar.zst
blockyard
```

The package uses system Qt libraries and requires an up-to-date Omarchy or Arch installation. Packages are currently unsigned; checksums verify the downloaded bytes. The release also includes source and a checksummed AUR recipe. AUR publication is pending account availability.

## Build and run

On Omarchy / Arch, the build dependencies are `base-devel`, `cmake`, `ninja`, `qt6-base` and `qt6-declarative`. The desktop should have a monospace font and `fontconfig`. Qt's Wayland platform plugin comes from `qt6-wayland`.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j 4
ctest --test-dir build --output-on-failure
./build/blockyard
```

Pass a folder to scan it instead of Home:

```sh
./build/blockyard /path/to/folder
```

This is a Linux application. Safe path resolution requires Linux 5.6 or later. Qt 6.4 or later and a C++20 compiler are required.

## Install for your user

```sh
cmake --install build --prefix "$HOME/.local"
```

Launch **Blockyard** from your application launcher, or run `~/.local/bin/blockyard`. No root privileges, autostart service, shell hooks or desktop configuration changes are required.

To uninstall, remove these installed files:

- `~/.local/bin/blockyard`
- `~/.local/share/applications/com.blockyard.app.desktop`
- `~/.local/share/icons/hicolor/scalable/apps/blockyard.svg`
- `~/.local/share/licenses/blockyard/LICENSE`

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

The scan root, mounted descendants, nested Btrfs subvolumes, incomplete scans, linked Git worktrees and recognized service/configuration data are protected. Scan other mounts separately. Blockyard does not run elevated, prune package stores, manage snapshots, or infer that an old folder is disposable. See [engine behavior and limits](docs/engine.md).

## Omarchy integration

Blockyard observes the active theme directory and repairs its watches when Omarchy replaces it. It reads colors as data and uses fontconfig for the active monospace font. Contrast adjustments keep labels readable. Temporary partial writes retain the last valid palette. See [theme compatibility](docs/theming.md).

A `--theme-home DIRECTORY` option lets tests load copied theme fixtures without altering the desktop. `--screenshot FILE` saves the app window after a scan and exits. Neither option mutates scanned files.

## Development

[Architecture](docs/architecture.md) explains the modules and the chosen limits. [Validation](docs/validation.md) records release checks. Destructive tests use owned temporary fixtures only.

[Release instructions](docs/releasing.md) describe the clean Arch build, package validation and publication process.

The original static design study is in `design/options.html`. Atlas is the implemented direction; the other layouts are proposals, not additional app modes.

## License

MIT. See [LICENSE](LICENSE). Qt and other runtime dependencies retain their own licenses.
