#!/usr/bin/env bash
# Builds mat-track (Release) and installs it for the current user:
#   ~/.local/bin/mat-track
#   ~/.local/share/applications/mat-track.desktop   (Mint menu entry)
#   ~/.local/share/icons/hicolor/scalable/apps/mat-track.svg
#
#   ./install.sh               build and install
#   ./install.sh --uninstall   remove exactly what was installed
#   PREFIX=/some/dir ./install.sh   install somewhere else
#
# Your data in ~/.local/share/mat-track/ is never touched.
set -euo pipefail
cd "$(dirname "$0")"
prefix="${PREFIX:-$HOME/.local}"
build=build-release

if [[ "${1:-}" == "--uninstall" ]]; then
  manifest="$build/install_manifest.txt"
  [[ -f "$manifest" ]] || { echo "nothing to uninstall (no $manifest)"; exit 1; }
  # The manifest has no final newline, so also take an unterminated last line.
  while IFS= read -r f || [[ -n "$f" ]]; do rm -fv -- "$f"; done < "$manifest"
  exit 0
fi

cmake -S . -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DMAT_TRACK_BUILD_TESTS=OFF \
  -DCMAKE_INSTALL_PREFIX="$prefix"
cmake --build "$build"
cmake --install "$build"
# Refresh the menu and icon caches where those tools exist.
command -v update-desktop-database >/dev/null && update-desktop-database "$prefix/share/applications" || true
command -v gtk-update-icon-cache >/dev/null && gtk-update-icon-cache -q -t "$prefix/share/icons/hicolor" || true
echo
echo "Installed to $prefix. Launch mat-track from the menu, or run: $prefix/bin/mat-track"
