# Validation

Release checks are performed against the native Qt application, not a web preview. Filesystem mutation tests use owned temporary fixtures. Desktop theme tests use copied files and never switch the real desktop theme.

## Automated checks

The CTest suite covers engine accounting and cleanup, Omarchy palette adaptation, asynchronous controller state, independent adversarial cleanup attempts, and the actual QML/treemap interface. Additional integrated application tests exercise the full scan/review/Trash/result flow with real filesystem fixtures.

Theme tests read every available stock palette on an Omarchy machine. All 22 installed stock palettes passed, including light themes. Synthetic tests also cover legacy ANSI and Alacritty formats, invalid intermediate files, directory replacement and symlink retargeting. Non-Omarchy CI runs the synthetic tests and explicitly skips the unavailable stock palette inventory.

An independent reviewer reproduced a newly created Git worktree marker bypass after review. Execution now rechecks ancestor markers, and the adversarial fixture verifies preservation. Other attempts cover ancestor and root replacement, symlink substitution, added hard links, failed Trash metadata writes, and stale review state.

Native UI tests check narrow-window map geometry, keyboard marking and review cancellation, a separate permanent-removal confirmation, and literal rendering of hostile-looking filename text. Filename bytes remain native in the engine; unrepresentable bytes and control characters are escaped for review.

## Manual and performance checks

Native Wayland launches and offscreen Qt rendering are used to inspect laptop-size and narrow tiled layouts. Light and dark fixture palettes are also rendered. The initial launch exposed a toolbar taking too much vertical space and a font binding loop; both were corrected and added to the relevant checks.

A read-only metadata scan of a representative real developer home covered approximately 2.2 million entries. The initial implementation took about 18 seconds and peaked at about 1.08 GiB RSS. This is an observation from this machine, not a throughput guarantee. Small-file counts, filesystem cache, storage, and permissions affect performance. Per-entry allocations and path processing were then reduced; final measurements are recorded with the release.

Mount creation/race tests requiring elevated privileges were not run on the user's machine. Mount-boundary behavior is checked in source through `openat2` restrictions and verified for existing scan boundaries. No real user data is removed in validation.
