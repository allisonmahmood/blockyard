# Releasing Blockyard

The application version is recorded in `CMakeLists.txt` and `src/main.cpp`. Update both for a new release and add notes under `docs/releases/vVERSION.md`.

1. Commit and push the release source. Wait for the normal build and test workflow.
2. Run **Package for Arch** on that exact commit. It archives only tracked, committed files, creates a checksummed PKGBUILD and `.SRCINFO`, then builds and runs all six tests as an unprivileged user in a clean Arch container.
3. Check the package audit output. The runner installs and launches the package before uploading release artifacts. Nothing is installed on the maintainer's computer.
4. Download the `blockyard-arch-release` artifact. Verify `SHA256SUMS`, package contents, version, dependency metadata and MIT license.
5. Create and push an annotated `vVERSION` tag on the tested commit. Publish a GitHub release using the corresponding notes and attach the source archive, Arch package, `arch-build` recipe archive and `SHA256SUMS`.
6. Download the public assets again and verify the checksums. Tags and release assets are immutable by convention; corrections get a new version.

The package workflow has read-only repository permissions and does not publish automatically. Publication uses the maintainer's existing GitHub access. Checksums detect changed downloads; they are not package signatures.

The generated source recipe is ready for a future AUR submission. Extract `PKGBUILD` and `.SRCINFO` from the `arch-build` archive and copy them into the AUR package repository when an account is available. The archive preserves the leading dot in `.SRCINFO`, which GitHub would remove from a standalone asset filename. Do not advertise an AUR install command until the package is actually published there.

For local archive preparation on Arch, run `bash scripts/prepare-arch-release.sh` from a committed checkout. It creates a new `dist/VERSION` directory and refuses to overwrite an existing one. Build there using `makepkg`; do not run it as root.

Packaging follows the [Arch PKGBUILD manual](https://man.archlinux.org/man/PKGBUILD.5.en). Release uploads use [GitHub CLI](https://cli.github.com/manual/gh_release_upload).
