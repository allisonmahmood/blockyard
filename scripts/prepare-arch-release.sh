#!/usr/bin/env bash
set -euo pipefail

# Archive only committed files, then bind the Arch recipe to those exact bytes.
cd "$(git rev-parse --show-toplevel)"
commit=$(git rev-parse HEAD)
version=$(git show "$commit:CMakeLists.txt" | sed -nE 's/^project\(Blockyard VERSION ([0-9.]+) LANGUAGES CXX\)$/\1/p')
if [[ ! $version =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
  printf 'Could not read a release version from CMakeLists.txt\n' >&2
  exit 1
fi

output="dist/$version"
mkdir -p dist
mkdir "$output"
git archive --format=tar --prefix="blockyard-$version/" "$commit" |
  gzip -n > "$output/blockyard-$version.tar.gz"
checksum=$(sha256sum "$output/blockyard-$version.tar.gz" | cut -d ' ' -f 1)
git show "$commit:packaging/arch/PKGBUILD.in" |
  sed -e "s/@VERSION@/$version/g" -e "s/@SOURCE_SHA256@/$checksum/g" > "$output/PKGBUILD"
(cd "$output" && makepkg --printsrcinfo > .SRCINFO)
printf 'Prepared %s from commit %s\n' "$output" "$commit"
