# Building and contributing

Blockyard is a native Qt 6 / Qt Quick application written in C++20. It uses CMake and Ninja.

## Build and run

On Omarchy / Arch, the build dependencies are `base-devel`, `cmake`, `ninja`, `qt6-base` and `qt6-declarative`. The desktop should have a monospace font and `fontconfig`. Qt's Wayland platform plugin comes from `qt6-wayland`, and SVG icon support comes from `qt6-svg`.

```sh
git clone https://github.com/allisonmahmood/blockyard.git
cd blockyard
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

## T3 Code

The repository's `t3.json` supplies the Blockyard icon and defaults new threads to separate worktrees. An explicit workspace preference in T3 Code's project settings overrides this default.

Install the build dependencies above before creating a worktree. The **Configure** action runs CMake automatically when T3 Code creates one. **Build** configures and compiles the project, and **Test** builds before running all CTest suites with offscreen Qt rendering. Each action uses the current checkout's ignored `build/` directory.

For a project already open in T3 Code, import the actions from this checkout in **Settings → Projects**.

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

## Contributing

Bug reports and focused contributions are welcome. When reporting a problem, include the Blockyard and Qt versions, what happened, and how to reproduce it. Use a small disposable example where possible, and redact personal paths from screenshots or logs.

- [Architecture](architecture.md) describes the application modules.
- [Storage engine](engine.md) documents accounting and cleanup behavior.
- [Theme integration](theming.md) explains Omarchy support.
- [Validation](validation.md) records the existing checks.
- [Release process](releasing.md) covers packaging and publication.

Filesystem mutation tests must use owned temporary fixtures. Theme tests use copied palettes instead of changing the live desktop theme.

The original interface study is in [design/options.html](../design/options.html). Atlas is the implemented direction.

[Back to Blockyard](../README.md)
