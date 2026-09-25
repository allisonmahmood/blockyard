# Blockyard

Build a native Qt 6 / Qt Quick application with C++20 and CMake. Allison explicitly ruled out Tauri. Preserve this stack decision.

The approved interface is Atlas in `design/options.html`. Omarchy's active theme and font must update the running application, including light palettes and theme-directory replacement. Read theme files as data and test with fixtures; do not switch the user's live desktop theme.

Filesystem mutation tests may touch only owned temporary fixtures. Keep the application unprivileged. Review and revalidate cleanup selections, never silently turn a failed Trash operation into permanent deletion, and never treat selected bytes as guaranteed freed space.

Keep dependencies and modules small. Use Qt's native APIs where they fit. No persistent scan database, background daemon, cloud service, automatic cleaner or continuous animation.

The repository is public under MIT. Keep machine observations, real file listings, personal paths and credentials out of published commits and screenshots.
