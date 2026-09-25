from pathlib import Path
import json

out = Path(__file__).parent
repo = 'https://github.com/allisonmahmood/blockyard'
raw = 'https://raw.githubusercontent.com/allisonmahmood/blockyard/a6bfbb0/design/readme/screenshots/'
logo = 'https://raw.githubusercontent.com/allisonmahmood/blockyard/main/assets/blockyard.svg'

def shot(name, alt):
    return f'![{alt}]({raw}{name}.png)'

install = f'''## Install

Download the x86_64 package and `SHA256SUMS` from the [latest release]({repo}/releases/latest). On an up-to-date Omarchy or Arch installation:

```sh
sha256sum --ignore-missing --check SHA256SUMS
sudo pacman -U ./blockyard-0.1.0-1-x86_64.pkg.tar.zst
```

Open **Blockyard** from your application launcher, or run `blockyard`.

Packages are currently unsigned. The release notes include verification details and source builds. AUR publication is planned when an account is available.
'''
footer = f'''## More

[Build from source]({repo}#build-and-run) · [Keyboard controls]({repo}#navigation) · [How cleanup works]({repo}/blob/main/docs/engine.md) · [Theme support]({repo}/blob/main/docs/theming.md) · [Report an issue]({repo}/issues)

Contributions and bug reports are welcome. [MIT licensed]({repo}/blob/main/LICENSE).
'''
# Existing section links above remain valid during preview. In the chosen README,
# build and navigation material will move to dedicated documentation pages.

A = f'''<div align="center">
<img src="{logo}" alt="Blockyard icon" width="64" height="64">

# Blockyard

**See what's taking up space.**

A visual disk explorer for Omarchy. Find the big folders, inspect what's inside, and review what to remove.

[Download for Omarchy]({repo}/releases/latest) · [Documentation]({repo}/blob/main/docs/architecture.md) · [Report a bug]({repo}/issues)

</div>

{shot('dark-hero','Blockyard showing a nested disk map and the size of a selected cache folder')}

*Tokyo Night. Screenshots use illustrative demo data.*

## Find the folders that matter

- See space as nested blocks. Bigger folders take up more of the map.
- Double-click to explore, or use the folder list and search to find a name.
- Compare space on disk, file sizes and file counts, including hidden files.
- Mark items for review, check their paths, then choose Trash or permanent removal.

## Matches your Omarchy theme

Change your theme in Omarchy and Blockyard updates while it's open. Colors and fonts follow your desktop, including light themes.

{shot('light-hero','The same Blockyard view using the Catppuccin Latte light theme')}

*Catppuccin Latte. Same app, same demo folders.*

## Review before removing

Blockyard shows the selected paths before acting. Trash keeps files recoverable; permanent removal has a separate confirmation. Nothing is selected for cleanup automatically.

<details>
<summary>See the cleanup review</summary>

{shot('dark-review','The review dialog lists the exact selected path before offering Trash or permanent removal')}

</details>

Trash usually retains disk space until emptied. Selected bytes can also differ from space freed because of snapshots or shared data. [Read how cleanup works]({repo}/blob/main/docs/engine.md).

{install}
{footer}'''

B = f'''# Blockyard

See your disk space. Explore a folder. Decide what to keep.

A native disk explorer for Omarchy, with an interactive map and a review step before cleanup.

[Download]({repo}/releases/latest) · [Installation](#install) · [Report a bug]({repo}/issues)

## 1. Find the big folders

Open a folder and let the map show where its space goes. Each block represents a file or folder; its area follows the selected size metric.

{shot('dark-hero','A disk map with code, media, caches, downloads and agent folders')}

*Actual interface with illustrative demo data. Tokyo Night theme.*

## 2. Explore the details

Double-click to go deeper. Turn on the folder list for names and precise sizes, or press `/` to search. Hidden files are included.

{shot('dark-folders','A size-sorted folder list alongside the disk map and selected folder details')}

## 3. Review what goes

Press `Space` to mark an item, then review the selected paths. Move them to Trash, or confirm permanent removal separately. Blockyard never decides that an old folder is disposable.

{shot('dark-review','Cleanup review showing a selected cache folder and explicit action choices')}

Trash keeps files recoverable and usually retains their space. Snapshots and shared blocks can also affect space freed. [Cleanup details]({repo}/blob/main/docs/engine.md).

## At home on Omarchy

Colors and fonts follow your active theme while the app is open. Light themes work too. Navigate with the mouse, arrows or `hjkl`.

<details>
<summary>See the light theme</summary>

{shot('light-hero','Blockyard in Catppuccin Latte')}

</details>

{install}
{footer}'''

C = f'''<div align="center">
<img src="{logo}" alt="Blockyard icon" width="56" height="56">

# Blockyard

**A disk map for your Omarchy desktop.**

Explore your storage in the theme you already use.

[Download]({repo}/releases/latest) · [Theme support]({repo}/blob/main/docs/theming.md) · [Source]({repo})

</div>

<table>
<tr><td><img src="{raw}dark-hero.png" alt="Blockyard in Tokyo Night"></td><td><img src="{raw}light-hero.png" alt="Blockyard in Catppuccin Latte"></td></tr>
<tr><td align="center">Tokyo Night</td><td align="center">Catppuccin Latte</td></tr>
</table>

*The same interface and illustrative demo data in two Omarchy themes. Click either screenshot for the full-size view.*

## Your theme, live

Blockyard follows Omarchy's active colors and monospace font. Switch themes and the open app updates. Dark, light and monochrome palettes are supported.

## Explore with the map or the keyboard

Big blocks show big folders. Select one for details, open it to go deeper, and use search or the folder list to reach small entries.

{shot('dark-folders','Blockyard with the folder list, treemap and inspector visible')}

| Key | Action |
| --- | --- |
| `Enter` / `l` | Open the selected folder |
| `Backspace` / `h` | Go to the parent folder |
| `j` / `k` | Move through entries |
| `/` | Find a name |
| `Space` | Mark an item for review |
| `Ctrl+Enter` | Review marked items |

## Cleanup stays in your hands

Review the exact paths before moving anything to Trash. Permanent removal requires its own confirmation. Trash keeps files recoverable and usually keeps their space occupied. [Cleanup details]({repo}/blob/main/docs/engine.md).

<details>
<summary>See the review dialog</summary>

{shot('dark-review','Blockyard asks which action to take on reviewed paths')}

</details>

{install}
{footer}'''

for letter, content in [('A', A), ('B', B), ('C', C)]:
    (out/f'{letter}.md').write_text(content)
    target=Path('.tools/readme-capture')/f'{letter}.json'
    target.write_text(json.dumps({'text':content, 'mode':'gfm', 'context':'allisonmahmood/blockyard'}))
    print(f'Wrote {letter}.md')
