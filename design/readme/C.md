<div align="center">
<img src="https://raw.githubusercontent.com/allisonmahmood/blockyard/main/assets/blockyard.svg" alt="Blockyard icon" width="56" height="56">

# Blockyard

**A disk map for your Omarchy desktop.**

Explore your storage in the theme you already use.

[Download](https://github.com/allisonmahmood/blockyard/releases/latest) · [Theme support](https://github.com/allisonmahmood/blockyard/blob/main/docs/theming.md) · [Source](https://github.com/allisonmahmood/blockyard)

</div>

<table>
<tr><td><img src="https://raw.githubusercontent.com/allisonmahmood/blockyard/a6bfbb0/design/readme/screenshots/dark-hero.png" alt="Blockyard in Tokyo Night"></td><td><img src="https://raw.githubusercontent.com/allisonmahmood/blockyard/a6bfbb0/design/readme/screenshots/light-hero.png" alt="Blockyard in Catppuccin Latte"></td></tr>
<tr><td align="center">Tokyo Night</td><td align="center">Catppuccin Latte</td></tr>
</table>

*The same interface and illustrative demo data in two Omarchy themes. Click either screenshot for the full-size view.*

## Your theme, live

Blockyard follows Omarchy's active colors and monospace font. Switch themes and the open app updates. Dark, light and monochrome palettes are supported.

## Explore with the map or the keyboard

Big blocks show big folders. Select one for details, open it to go deeper, and use search or the folder list to reach small entries.

![Blockyard with the folder list, treemap and inspector visible](https://raw.githubusercontent.com/allisonmahmood/blockyard/a6bfbb0/design/readme/screenshots/dark-folders.png)

| Key | Action |
| --- | --- |
| `Enter` / `l` | Open the selected folder |
| `Backspace` / `h` | Go to the parent folder |
| `j` / `k` | Move through entries |
| `/` | Find a name |
| `Space` | Mark an item for review |
| `Ctrl+Enter` | Review marked items |

## Cleanup stays in your hands

Review the exact paths before moving anything to Trash. Permanent removal requires its own confirmation. Trash keeps files recoverable and usually keeps their space occupied. [Cleanup details](https://github.com/allisonmahmood/blockyard/blob/main/docs/engine.md).

<details>
<summary>See the review dialog</summary>

![Blockyard asks which action to take on reviewed paths](https://raw.githubusercontent.com/allisonmahmood/blockyard/a6bfbb0/design/readme/screenshots/dark-review.png)

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
