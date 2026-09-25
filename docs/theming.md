# Omarchy theme integration

`Theme` exposes the active palette and monospace font to QML. Register one instance as the `theme` context property. All properties emit `changed` together; consumers should bind to properties rather than cache colors. `refresh()` rereads the palette and asynchronously resolves the font. The object also refreshes when the application becomes active.

The adapter reads `~/.local/state/omarchy/current/theme/colors.toml`, with `~/.config/omarchy/current/theme/colors.toml` as the legacy fallback. It supports current named colors, older `color0` through `color15` palettes, and primary/normal color sections from `alacritty.toml` when `colors.toml` is absent. Every installed stock Omarchy palette currently uses `colors.toml`.

Only six-digit hexadecimal scalar colors and the `light` or `dark` mode values are accepted. The parser permits single or double quotes, `#` or `0x` color prefixes, and trailing comments. It never executes theme scripts. Background and foreground are required; optional categories fall back to the accent, orange falls back to yellow. An absent or incomplete palette leaves the last valid colors in place. The built-in neutral palette applies before the first valid theme is available.

The theme watcher observes containing directories as well as files. After an event it waits 90 milliseconds, rereads the logical path, and reattaches watches. This handles Omarchy replacing its current theme directory, editors replacing files, and older installations retargeting symlinks. It watches the nearest existing parent when the theme does not exist yet. There is no periodic poll.

The adapter asks `fc-match monospace` for the active font through `QProcess` with fixed arguments. It does this on launch, focus, and watched changes. Font resolution is asynchronous and has a 1.5-second timeout. It watches the user fontconfig directory when present. It never changes fontconfig or installs an Omarchy hook.

Background and category hues follow the source palette. Raised surfaces are kept close to the background. Foreground, muted text, and accent are adjusted toward black or white when needed to reach a 4.5:1 contrast ratio against both backgrounds. Selection is adjusted to preserve foreground readability. Category colors are intended for fills and indicators, not body text. This also supports monochrome palettes and light themes.

`Theme(QObject*)` uses the current user's home. `Theme(QString homePath, QObject*)` provides an isolated home for tests. The adapter never applies a stock theme in place of a missing current theme. Tests copy palette data from installed stock themes into temporary homes, and exercise light/dark palettes, missing keys, legacy formats, temporary invalid data, directory replacement, and symlink replacement. They never write to the active desktop configuration.
