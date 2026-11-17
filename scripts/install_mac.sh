#!/bin/bash
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
SRC="$HERE/CRUMBS.vst3"
if [ ! -d "$SRC" ]; then
  echo "CRUMBS.vst3 not found next to this script."
  exit 1
fi
DEST="$HOME/Library/Audio/Plug-Ins/VST3"
echo "This will copy CRUMBS.vst3 to $DEST"
mkdir -p "$DEST"
cp -R "$SRC" "$DEST/"
echo "Done. Rescan plugins in FL Studio, or restart it."
