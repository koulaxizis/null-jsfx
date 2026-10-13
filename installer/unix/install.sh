#!/usr/bin/env bash
# NULL JSFX @@VERSION@@ installer for @@OS@@: pick the formats (JSFX, JSFX GFX, VST3, CLAP),
# the plugins (all, by group or one by one) and the extras, and it copies them where REAPER and
# your DAW look for them. The packages ride along at the end of this file.
#
#   bash NULL-JSFX-Installer-@@OS@@.sh               asks what to install
#   bash NULL-JSFX-Installer-@@OS@@.sh --help        all options (installing without questions too)
#   bash NULL-JSFX-Installer-@@OS@@.sh --uninstall   removes what it installed
#
# Free and open source (MIT), https://nulljsfx.tech
# Made by tools/build_installer.py from installer/unix/install.sh; works with bash 3.2 (macOS) too.
set -eu

VERSION='@@VERSION@@'
TARGET_OS='@@OS@@'
STUDIO='https://useoros.online/studio/'

# stem|name|group|has GFX|has VST3/CLAP
PLUGINS='@@PLUGINS@@'

# folder|file|plugins it uses
CHAINS='@@CHAINS@@'

GROUP_KEYS='one-slider essentials advanced'
FORMAT_KEYS='jsfx gfx vst3 clap'
EXTRA_KEYS='chains rack remote fonts'

case "$TARGET_OS" in
  macOS)
    REAPER_DIR="$HOME/Library/Application Support/REAPER"
    VST3_DIR="$HOME/Library/Audio/Plug-Ins/VST3"
    CLAP_DIR="$HOME/Library/Audio/Plug-Ins/CLAP"
    FONT_DIR="$HOME/Library/Fonts"
    DATA_DIR="$HOME/Library/Application Support/NULL JSFX"
    ;;
  *)
    REAPER_DIR="${XDG_CONFIG_HOME:-$HOME/.config}/REAPER"
    VST3_DIR="$HOME/.vst3"
    CLAP_DIR="$HOME/.clap"
    FONT_DIR="${XDG_DATA_HOME:-$HOME/.local/share}/fonts"
    DATA_DIR="${XDG_DATA_HOME:-$HOME/.local/share}/null-jsfx"
    ;;
esac

FORMATS='' GROUPS_SEL='' PICKS='' EXTRAS=''
formats_set=0 plugins_set=0 extras_set=0 reaper_set=0
yes=0 uninstall=0 force=0

say() { printf '%s\n' "$*"; }
die() { printf 'NULL JSFX installer: %s\n' "$*" >&2; exit 1; }
has() { case " $1 " in *" $2 "*) return 0 ;; esac; return 1; }
lower() { printf '%s' "$1" | tr 'A-Z_,' 'a-z- '; }

usage() {
  cat <<EOF
NULL JSFX $VERSION installer for $TARGET_OS

Without options it asks what to install. With any of these it asks only about the rest,
and with --yes it asks nothing (everything not given is installed in full).

  --formats LIST    jsfx, gfx, vst3, clap or all       e.g. --formats vst3,clap
  --groups LIST     one-slider, essentials, advanced or all
  --plugins LIST    single plugins, e.g. --plugins tilt,delay,parametric-eq (see --list)
  --extras LIST     chains (REAPER FX chains), rack (Rack VST3/CLAP), remote (tablet
                    remote for REAPER), fonts (Noto Sans for the GFX look), all or none
  --all             everything
  --reaper DIR      REAPER resource folder (default: $REAPER_DIR)
  --vst3-dir DIR    default: $VST3_DIR
  --clap-dir DIR    default: $CLAP_DIR
  -y, --yes         no questions
  --list            list the plugins and their groups
  --uninstall       remove everything this installer put on this computer
  -h, --help        this text

The same effects run in your browser, nothing to install: orOS Studio web plugins,
$STUDIO
EOF
}

plugin_rows() { printf '%s\n' "$PLUGINS"; }

list_plugins() {
  for g in $GROUP_KEYS; do
    say "$(group_title "$g"):"
    plugin_rows | while IFS='|' read -r stem name group gfx native; do
      if [ "$group" = "$g" ]; then printf '  %-22s %s\n' "$(lower "$stem")" "$name"; fi
    done
  done
  return 0
}

group_title() {
  case "$1" in one-slider) say "One Slider" ;; essentials) say "Essentials" ;; advanced) say "Advanced" ;; esac
}
group_count() { plugin_rows | awk -F'|' -v g="$1" '$3 == g' | wc -l | tr -d ' '; }

# --- options -------------------------------------------------------------------------------
need_arg() { [ $# -ge 2 ] || die "$1 needs a value (see --help)"; }
while [ $# -gt 0 ]; do
  case "$1" in
    --formats) need_arg "$@"; FORMATS="$(lower "$2")"; formats_set=1; shift ;;
    --groups)  need_arg "$@"; GROUPS_SEL="$GROUPS_SEL $(lower "$2")"; plugins_set=1; shift ;;
    --plugins) need_arg "$@"; PICKS="$PICKS $(lower "$2")"; plugins_set=1; shift ;;
    --extras)  need_arg "$@"; EXTRAS="$(lower "$2")"; extras_set=1; shift ;;
    --all) FORMATS=all GROUPS_SEL=all EXTRAS=all; formats_set=1 plugins_set=1 extras_set=1 ;;
    --reaper)   need_arg "$@"; REAPER_DIR="$2"; reaper_set=1; shift ;;
    --vst3-dir) need_arg "$@"; VST3_DIR="$2"; shift ;;
    --clap-dir) need_arg "$@"; CLAP_DIR="$2"; shift ;;
    -y|--yes) yes=1 ;;
    --uninstall) uninstall=1 ;;
    --force) force=1 ;;
    --list) list_plugins; exit 0 ;;
    -h|--help) usage; exit 0 ;;
    *) die "unknown option: $1 (see --help)" ;;
  esac
  shift
done

MANIFEST="$DATA_DIR/installed.txt"

# --- uninstall -----------------------------------------------------------------------------
if [ "$uninstall" = 1 ]; then
  [ -f "$MANIFEST" ] || die "nothing to remove: no $MANIFEST"
  n=0
  while IFS= read -r path; do
    [ -n "$path" ] || continue
    if [ -e "$path" ] || [ -L "$path" ]; then rm -rf "$path"; n=$((n + 1)); fi
  done < "$MANIFEST"
  for d in "$REAPER_DIR/Effects/null_jsfx/gfx" "$REAPER_DIR/Effects/null_jsfx" \
           "$REAPER_DIR/FXChains/NULL JSFX" "$REAPER_DIR/FXChains/NULL JSFX GFX" \
           "$VST3_DIR/NULL JSFX" "$CLAP_DIR/NULL JSFX"; do
    rmdir "$d" 2>/dev/null || true
  done
  rm -f "$MANIFEST" "$DATA_DIR/INSTALL.txt" "$DATA_DIR/Remote-INSTALL.txt"
  rmdir "$DATA_DIR" 2>/dev/null || true
  say "Removed $n NULL JSFX items."
  exit 0
fi

case "$(uname -s)" in Darwin) host=macOS ;; Linux) host=Linux ;; *) host="$(uname -s)" ;; esac
if [ "$host" != "$TARGET_OS" ] && [ "$force" = 0 ]; then
  die "this is the $TARGET_OS installer and this computer runs $host. Download the one for $host from https://nulljsfx.tech (or add --force)."
fi

# --- questions -----------------------------------------------------------------------------
interactive=0
if [ "$yes" = 0 ] && [ -t 0 ]; then interactive=1; fi
[ "$yes" = 1 ] || [ "$interactive" = 1 ] || [ "$formats_set$plugins_set$extras_set" = 111 ] ||
  die "no terminal to ask in: give --yes or the choices (see --help)"

use_whiptail=0
if [ "$interactive" = 1 ] && command -v whiptail >/dev/null 2>&1 && [ -n "${TERM:-}" ] && [ "${TERM:-}" != dumb ]; then
  use_whiptail=1
fi

# checklist TITLE TEXT DEFAULT_ON key label [key label...]: prints the chosen keys
checklist() {
  title="$1" text="$2" on="$3"; shift 3
  if [ "$use_whiptail" = 1 ]; then
    n=0
    args=()
    while [ $# -gt 0 ]; do
      state=OFF; if has "$on" "$1" || [ "$on" = all ]; then state=ON; fi
      args+=("$1" "$2" "$state"); n=$((n + 1)); shift 2
    done
    rows=$n; [ $rows -gt 16 ] && rows=16
    out="$(whiptail --title "NULL JSFX $VERSION: $title" --separate-output \
      --checklist "$text (space toggles, Enter confirms)" $((rows + 9)) 74 $rows "${args[@]}" 3>&1 1>&2 2>&3)" ||
      die "cancelled"
    printf '%s\n' "$out" | tr '\n' ' '
    return 0
  fi
  {
    say ""
    say "$title"
    say "$text"
    keys=() i=0
    while [ $# -gt 0 ]; do
      i=$((i + 1)); keys+=("$1")
      mark=' '; if has "$on" "$1" || [ "$on" = all ]; then mark='x'; fi
      printf '  %2d [%s] %s\n' "$i" "$mark" "$2"; shift 2
    done
    printf 'Numbers to install, e.g. "1 3" or "2-5" (Enter keeps the [x] ones, 0 = none): '
  } >&2
  read -r answer || answer=''
  if [ -z "$answer" ]; then
    for k in "${keys[@]}"; do if has "$on" "$k" || [ "$on" = all ]; then printf '%s ' "$k"; fi; done
    return 0
  fi
  for tok in $(printf '%s' "$answer" | tr ',' ' '); do
    case "$tok" in
      0) ;;
      *-*) a="${tok%-*}" b="${tok#*-}"
           case "$a$b" in *[!0-9]*|'') die "not a number range: $tok" ;; esac
           j=$a; while [ "$j" -le "$b" ]; do
             [ "$j" -ge 1 ] && [ "$j" -le "$i" ] && printf '%s ' "${keys[$((j - 1))]}"; j=$((j + 1)); done ;;
      *[!0-9]*) die "not a number: $tok" ;;
      *) [ "$tok" -ge 1 ] && [ "$tok" -le "$i" ] || die "no choice $tok"
         printf '%s ' "${keys[$((tok - 1))]}" ;;
    esac
  done
}

if [ "$interactive" = 1 ]; then
  say "NULL JSFX $VERSION for $TARGET_OS: free audio plugins, AUDIO WITHOUT NOISE."
  say "Want to try them first? They also run in your browser as web plugins in orOS Studio: $STUDIO"
fi

if [ "$formats_set" = 0 ]; then
  if [ "$interactive" = 1 ]; then
    FORMATS="$(checklist "Formats" "Which versions of the plugins?" all \
      jsfx "JSFX: REAPER, with REAPER's own sliders" \
      gfx "JSFX GFX: REAPER, with the NULL JSFX interface" \
      vst3 "VST3: any DAW" \
      clap "CLAP: any DAW")"
  else FORMATS=all; fi
fi
[ "$FORMATS" = all ] && FORMATS="$FORMAT_KEYS"
for f in $FORMATS; do has "$FORMAT_KEYS" "$f" || die "unknown format: $f (jsfx, gfx, vst3, clap)"; done

if [ "$plugins_set" = 0 ]; then
  if [ "$interactive" = 1 ]; then
    GROUPS_SEL="$(checklist "Plugins" "Which groups? Tick the last line to pick single plugins." "$GROUP_KEYS" \
      one-slider "One Slider ($(group_count one-slider)): one control, instant result" \
      essentials "Essentials ($(group_count essentials)): the everyday mixing toolbox" \
      advanced "Advanced ($(group_count advanced)): sound design and special processing" \
      pick "Pick single plugins")"
    if has "$GROUPS_SEL" pick; then
      on=''
      while IFS='|' read -r stem name group gfx native; do
        if has "$GROUPS_SEL" "$group"; then on="$on $stem"; fi
      done <<EOF
$PLUGINS
EOF
      args=()
      while IFS='|' read -r stem name group gfx native; do
        args+=("$stem" "$name ($(group_title "$group"))")
      done <<EOF
$PLUGINS
EOF
      PICKS="$(checklist "Plugins" "Which plugins?" "$on" "${args[@]}")"
      GROUPS_SEL=''
    fi
  else GROUPS_SEL=all; fi
fi

# the selected plugins, by stem
SELECTED=''
has "$GROUPS_SEL" all && GROUPS_SEL="$GROUP_KEYS"
for g in $GROUPS_SEL; do has "$GROUP_KEYS" "$g" || die "unknown group: $g (one-slider, essentials, advanced)"; done
has "$PICKS" all && GROUPS_SEL="$GROUP_KEYS" PICKS=''
known=''
while IFS='|' read -r stem name group gfx native; do
  key="$(lower "$stem")"
  known="$known $key"
  if has "$GROUPS_SEL" "$group" || has "$PICKS" "$key" || has "$PICKS" "$stem" || has "$PICKS" "$(lower "$name" | tr ' ' '-')"; then
    SELECTED="$SELECTED $stem"
  fi
done <<EOF
$PLUGINS
EOF
for p in $PICKS; do
  pk="$(lower "$p")"
  has "$known" "$pk" || plugin_rows | awk -F'|' '{print tolower($2)}' | tr ' ' '-' | grep -qx -- "$pk" ||
    die "unknown plugin: $p (see --list)"
done

if [ "$extras_set" = 0 ]; then
  if [ "$interactive" = 1 ]; then
    EXTRAS="$(checklist "Extras" "Anything else?" "chains rack fonts" \
      chains "FX chains for REAPER (installed when their plugins are)" \
      rack "Rack: up to 8 NULL JSFX effects in one VST3/CLAP" \
      remote "Tablet remote for REAPER (needs a few steps in REAPER)" \
      fonts "Noto Sans font, for the exact look of the GFX versions")"
  else EXTRAS=all; fi
fi
[ "$EXTRAS" = all ] && EXTRAS="$EXTRA_KEYS"
[ "$EXTRAS" = none ] && EXTRAS=''
for e in $EXTRAS; do has "$EXTRA_KEYS" "$e" || die "unknown extra: $e (chains, rack, remote, fonts)"; done

needs_reaper=0
if has "$FORMATS" jsfx || has "$FORMATS" gfx || has "$EXTRAS" remote; then needs_reaper=1; fi
if [ "$needs_reaper" = 1 ] && [ "$interactive" = 1 ] && [ "$reaper_set" = 0 ]; then
  if [ -d "$REAPER_DIR" ]; then note="found"; else note="not found yet; it is made when REAPER first runs"; fi
  printf '\nREAPER resource folder (%s)\n[%s]\nEnter keeps it, or type another path (portable REAPER): ' "$note" "$REAPER_DIR" >&2
  read -r answer || answer=''
  [ -n "$answer" ] && REAPER_DIR="$answer"
fi

count=0; for s in $SELECTED; do count=$((count + 1)); done
fmt_names=''
for f in $FORMATS; do
  case "$f" in jsfx) n=JSFX ;; gfx) n='JSFX GFX' ;; vst3) n=VST3 ;; clap) n=CLAP ;; esac
  fmt_names="${fmt_names:+$fmt_names, }$n"
done
say ""
say "Installing $count plugins as ${fmt_names:-nothing}${EXTRAS:+, with:$(printf ' %s' $EXTRAS)}"
[ "$needs_reaper" = 1 ] && say "  REAPER: $REAPER_DIR"
has "$FORMATS" vst3 && say "  VST3:   $VST3_DIR/NULL JSFX"
has "$FORMATS" clap && say "  CLAP:   $CLAP_DIR/NULL JSFX"
if [ "$interactive" = 1 ]; then
  printf 'Go ahead? [Y/n] ' >&2
  read -r answer || answer=''
  case "$answer" in [nN]*) say "Nothing installed."; exit 0 ;; esac
fi

# --- install -------------------------------------------------------------------------------
tmp="$(mktemp -d "${TMPDIR:-/tmp}/null-jsfx.XXXXXX")"
trap 'rm -rf "$tmp"' EXIT
line="$(grep -an '^__PAYLOAD_BELOW__$' "$0" | head -n 1 | cut -d: -f1)"
[ -n "$line" ] || die "this file is damaged (no packages inside); download it again"
tail -n "+$((line + 1))" "$0" | (cd "$tmp" && tar xzf -) || die "could not unpack; download the installer again"

mkdir -p "$DATA_DIR"
touch "$MANIFEST"
installed=0
put() {  # put SRC DEST: copy a file or bundle and remember it for --uninstall
  [ -e "$1" ] || return 0
  mkdir -p "$(dirname "$2")"
  rm -rf "$2"
  cp -R "$1" "$2"
  printf '%s\n' "$2" >> "$MANIFEST"
  installed=$((installed + 1))
}

J="$tmp/jsfx/null_jsfx" E="$REAPER_DIR/Effects/null_jsfx"
V="$tmp/native/VST3/NULL JSFX" C="$tmp/native/CLAP/NULL JSFX"
for stem in $SELECTED; do
  if has "$FORMATS" jsfx; then
    put "$J/$stem.jsfx" "$E/$stem.jsfx"
    put "$J/$stem.jsfx.rpl" "$E/$stem.jsfx.rpl"
  fi
  if has "$FORMATS" gfx; then
    put "$J/gfx/${stem}_gfx.jsfx" "$E/gfx/${stem}_gfx.jsfx"
    put "$J/gfx/${stem}_gfx.jsfx.rpl" "$E/gfx/${stem}_gfx.jsfx.rpl"
  fi
  if has "$FORMATS" vst3; then put "$V/null_jsfx_$stem.vst3" "$VST3_DIR/NULL JSFX/null_jsfx_$stem.vst3"; fi
  if has "$FORMATS" clap; then put "$C/null_jsfx_$stem.clap" "$CLAP_DIR/NULL JSFX/null_jsfx_$stem.clap"; fi
done
if has "$FORMATS" gfx && [ -n "$SELECTED" ]; then
  put "$J/gfx/null_gfx.jsfx-inc" "$E/gfx/null_gfx.jsfx-inc"
fi
if has "$EXTRAS" rack; then
  if has "$FORMATS" vst3; then put "$V/null_jsfx_rack.vst3" "$VST3_DIR/NULL JSFX/null_jsfx_rack.vst3"; fi
  if has "$FORMATS" clap; then put "$C/null_jsfx_rack.clap" "$CLAP_DIR/NULL JSFX/null_jsfx_rack.clap"; fi
fi
chains=0
if has "$EXTRAS" chains; then
  while IFS='|' read -r folder file uses; do
    [ -n "$folder" ] || continue
    case "$folder" in *GFX) has "$FORMATS" gfx || continue ;; *) has "$FORMATS" jsfx || continue ;; esac
    ok=1; for u in $uses; do has "$SELECTED" "$u" || ok=0; done
    [ "$ok" = 1 ] || continue
    put "$tmp/jsfx/FXChains/$folder/$file" "$REAPER_DIR/FXChains/$folder/$file"
    chains=$((chains + 1))
  done <<EOF
$CHAINS
EOF
fi
if has "$EXTRAS" remote; then
  put "$tmp/jsfx/Remote/reaper_www_root/null_jsfx.html" "$REAPER_DIR/reaper_www_root/null_jsfx.html"
  put "$tmp/jsfx/Remote/Scripts/null_jsfx_remote_bridge.lua" "$REAPER_DIR/Scripts/null_jsfx_remote_bridge.lua"
  cp "$tmp/jsfx/Remote/INSTALL.txt" "$DATA_DIR/Remote-INSTALL.txt"
fi
if has "$EXTRAS" fonts && has "$FORMATS" gfx; then
  for f in "$tmp/jsfx/fonts/"*.ttf; do put "$f" "$FONT_DIR/$(basename "$f")"; done
  [ "$TARGET_OS" = Linux ] && command -v fc-cache >/dev/null 2>&1 && fc-cache -f "$FONT_DIR" >/dev/null 2>&1 || true
fi
cp "$tmp/jsfx/INSTALL.txt" "$DATA_DIR/INSTALL.txt"
sort -u "$MANIFEST" > "$MANIFEST.new" && mv "$MANIFEST.new" "$MANIFEST"

if [ "$TARGET_OS" = macOS ] && command -v xattr >/dev/null 2>&1; then
  for d in "$VST3_DIR/NULL JSFX" "$CLAP_DIR/NULL JSFX"; do
    [ -d "$d" ] && xattr -dr com.apple.quarantine "$d" 2>/dev/null || true
  done
fi

say ""
if [ "$chains" -gt 0 ]; then say "Done: $installed files installed, $chains of them FX chains."
else say "Done: $installed files installed."; fi
if [ "$needs_reaper" = 1 ]; then
  say "REAPER: open the FX browser and press F5 (or restart REAPER) to see them."
fi
if has "$FORMATS" vst3 || has "$FORMATS" clap; then
  say "Other DAWs: rescan plugins. Manufacturer: NULL JSFX."
fi
has "$EXTRAS" remote && say "Tablet remote: finish the steps in $DATA_DIR/Remote-INSTALL.txt (from step 4)."
say "Remove it all later with: bash $(basename "$0") --uninstall"
say "Try the effects in a browser too, as web plugins in orOS Studio: $STUDIO"
exit 0
__PAYLOAD_BELOW__
