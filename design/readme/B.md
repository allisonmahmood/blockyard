# Blockyard

See your disk space. Explore a folder. Decide what to keep.

A native disk explorer for Omarchy, with an interactive map and a review step before cleanup.

[Download](https://github.com/allisonmahmood/blockyard/releases/latest) · [Installation](#install) · [Report a bug](https://github.com/allisonmahmood/blockyard/issues)

## 1. Find the big folders

Open a folder and let the map show where its space goes. Each block represents a file or folder; its area follows the selected size metric.

![A disk map with code, media, caches, downloads and agent folders](https://raw.githubusercontent.com/allisonmahmood/blockyard/a6bfbb0/design/readme/screenshots/dark-hero.png)

*Actual interface with illustrative demo data. Tokyo Night theme.*

## 2. Explore the details

Double-click to go deeper. Turn on the folder list for names and precise sizes, or press `/` to search. Hidden files are included.

![A size-sorted folder list alongside the disk map and selected folder details](https://raw.githubusercontent.com/allisonmahmood/blockyard/a6bfbb0/design/readme/screenshots/dark-folders.png)

## 3. Review what goes

Press `Space` to mark an item, then review the selected paths. Move them to Trash, or confirm permanent removal separately. Blockyard never decides that an old folder is disposable.

![Cleanup review showing a selected cache folder and explicit action choices](https://raw.githubusercontent.com/allisonmahmood/blockyard/a6bfbb0/design/readme/screenshots/dark-review.png)

Trash keeps files recoverable and usually retains their space. Snapshots and shared blocks can also affect space freed. [Cleanup details](https://github.com/allisonmahmood/blockyard/blob/main/docs/engine.md).

## At home on Omarchy

Colors and fonts follow your active theme while the app is open. Light themes work too. Navigate with the mouse, arrows or `hjkl`.

<details>
<summary>See the light theme</summary>

![Blockyard in Catppuccin Latte](https://raw.githubusercontent.com/allisonmahmood/blockyard/a6bfbb0/design/readme/screenshots/light-hero.png)

</details>

## Install

Download the x86_64 package and `SHA256SUMS` from the [latest release](https://github.com/allisonmahmood/blockyard/releases/latest). On an up-to-date Omarchy or Arch installation:

```sh
sha256sum --ignore-missing --check SHA256SUMS
sudo pacman -U ./blockyard-0.1.0-1-x86_64.pkg.tar.zst
```

Open **Blockyard** from your application launcher, or run `blockyard`.

Packages are currently unsigned. The release notes include verification details and source builds. AUR publication is planned when an account is available.

## More

[Build from source](https://github.com/allisonmahmood/blockyard#build-and-run) · [Keyboard controls](https://github.com/allisonmahmood/blockyard#navigation) · [How cleanup works](https://github.com/allisonmahmood/blockyard/blob/main/docs/engine.md) · [Theme support](https://github.com/allisonmahmood/blockyard/blob/main/docs/theming.md) · [Report an issue](https://github.com/allisonmahmood/blockyard/issues)

Contributions and bug reports are welcome. [MIT licensed](https://github.com/allisonmahmood/blockyard/blob/main/LICENSE).
