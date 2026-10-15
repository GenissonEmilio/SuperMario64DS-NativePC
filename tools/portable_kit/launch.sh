#!/usr/bin/env bash
# SM64DS PC port: start it under Proton (or plain Wine) on Linux or a Steam Deck.
#
#   bash launch.sh              unpack the game data if needed, then open the launcher
#   bash launch.sh --game       skip the launcher and start the game itself
#   bash launch.sh --safe       force the software picture (no Direct3D 11 needed)
#   bash launch.sh --log        also write a Proton log (steam-<id>.log in your home folder)
#   bash launch.sh --dry-run    print what it would run and stop
#
# Put your own European Super Mario 64 DS .nds in the "PLACE YOUR ROM HERE" folder first.
#
# Environment you can set instead of editing this file:
#   PROTON=/path/to/proton              the "proton" script of the Proton you want
#   STEAM_COMPAT_DATA_PATH=/some/dir    where the Windows prefix lives
#                                       (default: .proton-prefix next to this script)
#
# Tested by the author: only the unpack step (python3) and this script's own logic.
# Running the game under Proton has been confirmed by one player with Proton
# Experimental on Arch/KDE/Wayland/NVIDIA; nothing else is confirmed. See README.txt.

set -u
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$HERE" || exit 1

MODE=launcher
SAFE=0
LOG=0
DRY=0
for a in "$@"; do
  case "$a" in
    --game)    MODE=game ;;
    --launcher) MODE=launcher ;;
    --safe)    SAFE=1 ;;
    --log)     LOG=1 ;;
    --dry-run) DRY=1 ;;
    -h|--help) sed -n '2,19p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
    *) echo "Unknown option: $a (try --help)"; exit 2 ;;
  esac
done

say() { printf '%s\n' "$*"; }
die() { printf '\n%s\n\n' "$*" >&2; exit 1; }

# 1. The game data. The launcher unpacks it with Windows PowerShell, which a Proton
#    prefix does not have, so it is unpacked here with python3 instead (same files,
#    byte for byte).
need=0
for f in build/assets/files.tsv build/assets/handles.tsv build/assets/nitrofs.tsv \
         build/assets/nitrofs_fnt.bin build/assets/nitrofs_fat.bin build/assets/nitrofs_ovt9.bin \
         build/assets/romdata.bin build/assets/romdata.manifest \
         extracted/dsd/files/data/sound_data.sdat; do
  [ -f "$f" ] || need=1
done
if [ "$need" = 1 ]; then
  say "First run: unpacking the game data from your cartridge dump."
  command -v python3 >/dev/null 2>&1 || die "python3 was not found. Install Python 3 (SteamOS and most distros have it), then run this again."
  [ -f extract_assets.py ] || die "extract_assets.py is missing from this folder. The kit is incomplete."
  python3 extract_assets.py --destination "$HERE" | grep -v '^##PROGRESS' || true
  [ -f build/assets/romdata.bin ] || die "The game data could not be unpacked. The lines above say why."
fi

# 2. Find something to run Windows programs with.
RUNNER=""
KIND=""
if [ -n "${PROTON:-}" ]; then
  [ -f "$PROTON" ] || die "PROTON is set to $PROTON but that file does not exist."
  RUNNER="$PROTON"; KIND=proton
else
  STEAM_ROOTS=("$HOME/.local/share/Steam" "$HOME/.steam/steam" "$HOME/.steam/root" \
               "$HOME/.var/app/com.valvesoftware.Steam/.local/share/Steam")
  for root in "${STEAM_ROOTS[@]}"; do
    for d in "$root"/steamapps/common/Proton*Experimental* "$root"/compatibilitytools.d/*; do
      if [ -f "$d/proton" ]; then RUNNER="$d/proton"; KIND=proton; STEAM_ROOT="$root"; break 2; fi
    done
  done
fi
if [ -z "$RUNNER" ] && command -v umu-run >/dev/null 2>&1; then RUNNER="umu-run"; KIND=umu; fi
if [ -z "$RUNNER" ] && command -v wine >/dev/null 2>&1; then RUNNER="wine"; KIND=wine; fi
[ -n "$RUNNER" ] || die "No Proton or Wine found. In Steam, install \"Proton Experimental\" (Library, filter to Tools), or set PROTON=/path/to/proton and run this again."

# 3. The Windows prefix lives next to the kit so it can be thrown away on its own.
PREFIX="${STEAM_COMPAT_DATA_PATH:-$HERE/.proton-prefix}"
mkdir -p "$PREFIX" 2>/dev/null || die "Cannot create $PREFIX"

# 4. What to start.
if [ "$MODE" = game ]; then
  TARGET="$HERE/walk_window.exe"
  # The launcher normally sets this; a hand start has to.
  export SM64DS_ASSET_ROOT="Z:${HERE//\//\\}"
else
  TARGET="$HERE/SM64DSLauncher.exe"
fi
[ -f "$TARGET" ] || die "$(basename "$TARGET") is missing from this folder. The kit is incomplete."

# 5. --safe: the software picture, the plain Windows present path, no presenter thread.
#    Each of these is the game's own override; the launcher passes them on to the game.
if [ "$SAFE" = 1 ]; then
  export SM64DS_RENDERER=0 SM64DS_PRESENT_BACKEND=0 SM64DS_PRESENTER=0
fi
[ "$LOG" = 1 ] && export PROTON_LOG=1 WINEDEBUG=err+all,warn+module
export PROTON_LOG_DIR="${PROTON_LOG_DIR:-$HOME}"

case "$KIND" in
  proton)
    export STEAM_COMPAT_DATA_PATH="$PREFIX"
    export STEAM_COMPAT_CLIENT_INSTALL_PATH="${STEAM_ROOT:-${STEAM_COMPAT_CLIENT_INSTALL_PATH:-$HOME/.local/share/Steam}}"
    CMD=("$RUNNER" run "$TARGET") ;;
  umu)
    export WINEPREFIX="$PREFIX" PROTONPATH="${PROTONPATH:-GE-Proton}"
    CMD=("$RUNNER" "$TARGET") ;;
  wine)
    export WINEPREFIX="$PREFIX"
    CMD=("$RUNNER" "$TARGET") ;;
esac

say "Runner : $KIND ($RUNNER)"
say "Prefix : $PREFIX"
say "Start  : $(basename "$TARGET")$([ "$SAFE" = 1 ] && echo '  (--safe)')"
if [ "$DRY" = 1 ]; then
  printf 'Command:'; printf ' %q' "${CMD[@]}"; printf '\n'
  exit 0
fi
exec "${CMD[@]}"
