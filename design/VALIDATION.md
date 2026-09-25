# Design validation

Checked 2026-09-25.

- Generated the single-file HTML from invented, consistent storage fixtures.
- Passed the PatchPage HTML validator before publication.
- Rendered all three application layouts in local headless Chromium using Playwright.
- Inspected screenshots at a 1280×800 browser viewport and at 1440×1000 during the first pass.
- Fixed a clipped review tray, overlapping nested folder labels, inconsistent list selection, and an inspector that consumed excessive vertical space.
- Final mockup sizes at a 1280px-wide page: Atlas 1200×678; Navigator and Reclaim 1200×654. These fit inside the laptop's logical display as standalone window designs.
- Checked the gallery at 390×844. The page has no horizontal overflow; wide static app previews scroll inside their own frame.
- Inspected the cleanup review cards and a light-theme color study.
- The gallery contains no scripts, live controls, real filenames, private URLs, local filesystem paths or measured laptop usage.

The PNGs in this folder are visual QA artifacts. They do not establish application behavior or WebKitGTK compatibility. Actual application testing and narrow-window layout behavior belong to implementation.
