# Design directions

Working name: Blockyard. This phase contains static proposals only. All storage figures and example folders in the published mockups are invented.

## House style

Follow Omarchy's installed Overtaking Landscape palette and JetBrainsMono Nerd Font. The application should adopt the active palette and font, including light themes, while preserving readable contrast. The Omarchy brief overrides the general PatchPage style.

Base tokens: background `#030800`, raised background `#1c211a`, foreground `#e8f6ed`, secondary foreground `#aeb9b2`, accent `#4694e4`, selected fill `#1a3144`. Category accents come from the theme's blue, cyan, green, orange, and yellow. Never interpret a category color as permission to delete.

Typography: JetBrainsMono Nerd Font for navigation, folders, measurements and prose. Tabular measurements, restrained weight, left alignment. App labels 13–15px, folder names 14px, selection heading 24px. Native desktop density rather than a marketing dashboard.

No idle animation, gradients, glass effects, wallpaper imitation, decorative charts, or floating cards inside the application. Borders indicate hierarchy, selection, or panels. Treemap area means bytes. Category color means folder kind. Hatching means an explicitly marked review candidate, never automatically safe to remove.

## A. Atlas

The treemap occupies most of the window, with a persistent inspector and a short cleanup review queue. Closest to the reference. Recommended for exploring where space went.

```text
breadcrumb                      size / age / depth
treemap, nested folders                   inspector
treemap                                   candidates
keyboard help                         scan coverage
```

Click selects, double-click or Enter drills in, breadcrumb or Backspace goes up. Space adds a selection to review. A compact review sheet explains the action before any write.

## B. Navigator

A conventional size-sorted folder list and a treemap remain visible together. Folder names stay easy to find even when their rectangles are tiny.

```text
breadcrumb                               rescan
folder list        treemap of selected folder
folder list        selection details and review action
keyboard help                           scan status
```

Recommended for people who navigate by names. The map loses some area but every visual action has an equivalent list action.

## C. Reclaim

A ranked review list leads, with the selected candidate located on a smaller treemap. Most direct route to deliberate cleanup.

```text
scope                          map / review modes
ranked candidates             context treemap
ranked candidates             action explanation
review tray                   measured disk space
```

Recommendations must state evidence and uncertainty. Old does not mean unused. Candidates are opt-in. This direction asks more of classification than A or B and should not imply an autonomous cleaner.

## Critique before implementation

All three use the same palette and dataset so the choice is about interaction rather than color. A large metric dashboard would waste the space needed for the treemap, so the disk summary stays compact. C initially suggested a single total of reclaimable bytes; replace it with bytes selected for review because trash, snapshots and shared blocks make that promise unsound. Use a light-theme preview to demonstrate the theme contract without changing the user's desktop.

Build and publish the static proposals. Wait for Allison's design pick and macro product answers before creating application components.
