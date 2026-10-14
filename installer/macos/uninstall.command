#!/bin/bash
# Removes what the NULL JSFX macOS installer put on this computer (double-click it in Finder).
# The factory presets added to preset files you already had stay; your own presets stay too, and
# so does the Noto Sans font (you may have had it before, other apps may use it).
# The NULL JSFX Hub can do the same (Settings > Uninstall) and more.
set -u
N="$HOME/Library/Application Support/NULL JSFX"
[ -d "$N/receipts" ] || { echo "Nothing to remove: no $N/receipts"; exit 0; }
printf 'Remove the NULL JSFX plugins this installer put on this Mac? [y/N] '
read -r answer
case "$answer" in [yY]*) ;; *) echo "Nothing removed."; exit 0 ;; esac
n=0
while IFS= read -r rel; do
  case "$rel" in ''|Library/Fonts/*) continue ;; esac
  if [ -e "$HOME/$rel" ]; then rm -rf "$HOME/$rel"; n=$((n + 1)); fi
done < <(cat "$N/receipts/"*.txt | sort -u)
A="$HOME/Library/Application Support/REAPER"
for d in "$A/Effects/null_jsfx/gfx" "$A/Effects/null_jsfx" "$A/FXChains/NULL JSFX" "$A/FXChains/NULL JSFX GFX" \
         "$HOME/Library/Audio/Plug-Ins/VST3/NULL JSFX" "$HOME/Library/Audio/Plug-Ins/CLAP/NULL JSFX"; do
  rmdir "$d" 2>/dev/null || true
done
for id in $(pkgutil --volume "$HOME" --pkgs='tech\.nulljsfx\.pkg\..*' 2>/dev/null); do
  pkgutil --volume "$HOME" --forget "$id" > /dev/null 2>&1 || true
done
rm -rf "$N"
echo "Removed $n NULL JSFX items."
