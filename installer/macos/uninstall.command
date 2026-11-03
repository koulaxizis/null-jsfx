#!/bin/bash
# Removes what the NULL JSFX macOS installer put on this computer (double-click it in Finder).
# The lines it added to REAPER's Scripts/__startup.lua go. The factory presets added to preset
# files you already had stay; your own presets stay too, and so does the Noto Sans font (you may
# have had it before, other apps may use it).
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
# our lines in REAPER's Scripts/__startup.lua (the remote's bridge); the user's own stay
if [ -f "$N/startup.txt" ] && su="$(cat "$N/startup.txt")" && [ -f "$su" ]; then
  awk '/^-- BEGIN NULL JSFX Remote/ { s = 1 } !s { print } /^-- END NULL JSFX Remote/ { s = 0 }' "$su" > "$su.null-jsfx"
  if grep -q '[^[:space:]]' "$su.null-jsfx"; then mv "$su.null-jsfx" "$su"; else rm -f "$su.null-jsfx" "$su"; fi
fi
for id in $(pkgutil --volume "$HOME" --pkgs='tech\.nulljsfx\.pkg\..*' 2>/dev/null); do
  pkgutil --volume "$HOME" --forget "$id" > /dev/null 2>&1 || true
done
rm -rf "$N"
echo "Removed $n NULL JSFX items."
