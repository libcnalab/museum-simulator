#!/usr/bin/env bash
# The real file dialogs and message boxes of dev-dialogs, answered the way a person answers them, on a private desktop:
# xdg-desktop-portal with its GTK backend runs on a private session bus (dbus-run-session with a bus configuration that can
# start nothing but the portal services), the museum and the dialogs run on a private Xvfb, and xdotool types the paths
# into the GTK file chooser and clicks message box buttons with the pointer. HOME, the XDG directories and the runtime
# directory point into build/dialogs-check, so GTK reads and writes no personal settings, bookmarks or recent files.
#   1. FileDialog.ShowOpenFile/ShowSaveFile/ShowOpenFolder through the portal return exactly the typed path; Escape cancels.
#   2. MessageBox.ShowSimple is dismissed with a click on its button; MessageBox.Show reports each of its three buttons. SDL
#      shows these through Zenity when it is installed (a GTK dialog).
#   3. A second museum without Zenity on its PATH gets SDL's own X11 message box, where the keyboard chooses: Return the
#      first button, Escape the last (CNA marks both; Zenity ignores such hints, docs/devices-ext.md).
#   tools/dialogs_check.sh      exit 0 when every check passed; 77 when a prerequisite is absent
set -u
cd "$(dirname "$0")/.." || exit 2
bin=$PWD/${MUSEUM_BIN:-${BUILD_DIR:-build}/bin/cna-3d-museum}
[ -x "$bin" ] || { echo "FAIL: $bin is not built"; exit 1; }
for tool in Xvfb xdotool dbus-run-session import; do command -v $tool > /dev/null || { echo "skipped: $tool is not installed"; exit 77; }; done
for program in xdg-desktop-portal xdg-desktop-portal-gtk xdg-permission-store; do
  [ -x /usr/libexec/$program ] || { echo "skipped: /usr/libexec/$program is not installed"; exit 77; }
done
/usr/bin/python3 -c "import PIL" 2> /dev/null || { echo "skipped: the system Python has no PIL (python3-pil)"; exit 77; }
dir=$PWD/${BUILD_DIR:-build}/dialogs-check
rm -rf "$dir"
mkdir -p "$dir/home/.config/xdg-desktop-portal" "$dir/home/files/folder" "$dir/run" "$dir/services"
chmod 700 "$dir/run"
cp Content/Textures/checker.png "$dir/home/files/picked.png"
# The portal picks its backend from portals.conf: only the GTK one, whatever the real desktop is.
printf '[preferred]\ndefault=gtk\n' > "$dir/home/.config/xdg-desktop-portal/portals.conf"
for service in org.freedesktop.portal.Desktop org.freedesktop.impl.portal.desktop.gtk org.freedesktop.impl.portal.PermissionStore; do
  grep -v '^SystemdService=' /usr/share/dbus-1/services/$service.service > "$dir/services/$service.service"
done
cat > "$dir/bus.conf" <<EOF
<!DOCTYPE busconfig PUBLIC "-//freedesktop//DTD D-Bus Bus Configuration 1.0//EN" "http://www.freedesktop.org/standards/dbus/1.0/busconfig.dtd">
<busconfig>
  <type>session</type>
  <listen>unix:dir=$dir/run</listen>
  <servicedir>$dir/services</servicedir>
  <policy context="default"><allow send_destination="*" eavesdrop="true"/><allow eavesdrop="true"/><allow own="*"/></policy>
</busconfig>
EOF
log=$dir/museum.log driver=$dir/driver.log

exec 3> "$dir/display"
Xvfb -displayfd 3 -screen 0 1600x900x24 > /dev/null 2>&1 & xvfb=$!
trap 'kill $xvfb 2> /dev/null' EXIT
for _ in $(seq 1 50); do [ -s "$dir/display" ] && break; sleep 0.1; done
exec 3>&-
n=$(tr -dc '0-9' < "$dir/display")
[ -n "$n" ] && kill -0 $xvfb 2> /dev/null || { echo "FAIL: Xvfb did not start"; exit 1; }

# The portal services are started by the private bus and inherit this environment.
env -u WAYLAND_DISPLAY -u DBUS_SESSION_BUS_ADDRESS DISPLAY=:$n HOME="$dir/home" XDG_CONFIG_HOME="$dir/home/.config" \
  XDG_DATA_HOME="$dir/home/.local/share" XDG_CACHE_HOME="$dir/home/.cache" XDG_STATE_HOME="$dir/home/.local/state" XDG_RUNTIME_DIR="$dir/run" \
  XDG_CURRENT_DESKTOP=museum-check GDK_BACKEND=x11 GIO_USE_VFS=local GIO_USE_VOLUME_MONITOR=unix GSETTINGS_BACKEND=memory NO_AT_BRIDGE=1 \
  dbus-run-session --config-file="$dir/bus.conf" -- bash -s "$bin" "$dir" "$log" "$driver" 2> "$dir/bus.log" <<'INNER'
bin=$1 dir=$2 log=$3 driver=$4
case "$DBUS_SESSION_BUS_ADDRESS" in *"$dir/run"*) ;; *) echo "FAIL: not the private session bus" > "$driver"; exit 1 ;; esac
say() { echo "$*" >> "$driver"; }
script="wait:30"
script+=";press:FileDialog.ShowOpenFile;await:FileDialog.ShowOpenFile: 1,3000"
script+=";press:FileDialog.ShowSaveFile;await:FileDialog.ShowSaveFile: 1,3000"
script+=";press:FileDialog.ShowOpenFolder;await:FileDialog.ShowOpenFolder: 1,3000"
script+=";press:FileDialog.ShowOpenFile;await:FileDialog.ShowOpenFile: cancelled,3000"
script+=";press:MessageBox.ShowSimple;wait:10"
script+=";press:MessageBox.Show (Yes;wait:10;press:MessageBox.Show (Yes;wait:10;press:MessageBox.Show (Yes;wait:10;stats;quit"
LIBGL_ALWAYS_SOFTWARE=1 LP_NUM_THREADS=${LP_NUM_THREADS:-3} SDL_VIDEODRIVER=x11 SDL_FILE_DIALOG_DRIVER=portal \
  timeout -k 10 "${MUSEUM_TIMEOUT:-300}" "$bin" --room dev-dialogs --no-audio --fixed-step 0.02 --script "$script" > "$log" 2>&1 &
museum=$!

window() {   # window NAME-REGEX: the id of the first visible window so named, waiting up to 60 s (empty when none appears)
  for _ in $(seq 1 120); do
    w=$(xdotool search --onlyvisible --name "$1" 2> /dev/null | head -1)
    [ -n "$w" ] && { echo "$w"; return; }
    kill -0 $museum 2> /dev/null || return
    sleep 0.5
  done
}
gone() { for _ in $(seq 1 20); do xdotool getwindowname "$1" > /dev/null 2>&1 || return 0; sleep 0.25; done; return 1; }
shot() { import -window root "$dir/$1.png" 2> /dev/null; }
click_at() {   # click_at WINDOW X Y: a pointer click at window coordinates
  xdotool mousemove --window "$1" "$2" "$3" sleep 0.2 click 1
}
answer() {   # answer TITLE-REGEX PATH: types PATH into the GTK chooser's location field and confirms it, as a person does
  local w; w=$(window "$1")
  [ -n "$w" ] || { say "no window named $1"; shot missing-dialog; return; }
  say "window $1: $w"
  xdotool windowfocus --sync "$w"
  # The save dialog has a name field with the focus already; the others open the location field with Ctrl+L.
  [ "$1" = "^Save File\$" ] && xdotool key --clearmodifiers ctrl+a || xdotool key --clearmodifiers ctrl+l
  sleep 0.5
  xdotool type --delay 15 "$2"
  sleep 0.8
  xdotool key Return
  gone "$w" && { say "$1 closed by Return"; return; }
  # The location field can keep Return for its completion; the dialog's default button is at its lower right.
  eval "$(xdotool getwindowgeometry --shell "$w")"
  shot before-click
  click_at "$w" $((WIDTH - 45)) $((HEIGHT - 24))
  gone "$w" && say "$1 closed by its button" || { say "$1 still open"; shot stuck-dialog; }
}
buttons() {   # buttons WINDOW: "x,y" centres of the message box's buttons in reading order, from a picture of the window
  import -window "$1" "$dir/box.png" 2> /dev/null
  /usr/bin/python3 -I - "$dir/box.png" <<'PY'
import sys
from PIL import Image
image = Image.open(sys.argv[1]).convert("RGB")
w, h = image.size
px = image.load()
background = px[8, h // 2]
def differs(x, y): return sum(abs(a - b) for a, b in zip(px[x, y], background)) > 12
def runs(y):   # filled spans narrower than the window (its frame spans all of it) and wider than a glyph
    out, start = [], None
    for x in range(w + 1):
        d = x < w and differs(x, y)
        if d and start is None: start = x
        if not d and start is not None:
            if 60 < x - start < w - 20: out.append((start, x))
            start = None
    return out
# The buttons are the filled boxes, side by side or stacked when the text is narrow: each band of rows with such spans
# is one row of buttons; its spans just above its lower edge (below the labels, above the rounded corners) are the buttons.
bands, start = [], None
for y in range(h // 3, h):
    if runs(y):
        if start is None: start = y
    elif start is not None:
        bands.append((start, y - 1)); start = None
centres = []
for top, bottom in bands:
    if bottom - top < 12: continue
    centres += [f"{(a + b) // 2},{(top + bottom) // 2}" for a, b in runs(bottom - 6)]
print(" ".join(centres))
PY
}
dismiss() {   # dismiss INDEX LABEL: clicks the INDEXth button (in reading order) of the next message box
  local w; w=$(window "^CNA Museum message\$")
  [ -n "$w" ] || { say "no message box"; shot missing-box; return; }
  local before; before=$(grep -ac 'MessageBox.Show' "$log")
  # The box draws itself after it is mapped: picture it until its buttons are there.
  local centres=()
  for _ in $(seq 1 20); do sleep 0.25; read -r -a centres <<< "$(buttons "$w")"; [ "${#centres[@]}" -gt 0 ] && break; done
  say "message box $w buttons: ${centres[*]}"
  cp "$dir/box.png" "$dir/box-$box.png"; box=$((box + 1))
  if [ "${#centres[@]}" -gt "$1" ]; then
    click_at "$w" "${centres[$1]%,*}" "${centres[$1]#*,}"
    say "clicked button $1 at ${centres[$1]} ($2)"
  else
    say "button $1 not found"
  fi
  # A closed box is a new result line in the log (window ids are reused, so the window itself proves nothing).
  for _ in $(seq 1 20); do [ "$(grep -ac 'MessageBox.Show' "$log")" -gt "$before" ] && return; sleep 0.25; done
  say "message box still open: closing it ($2)"; shot stuck-box
  xdotool windowclose "$w" 2> /dev/null
}
keyed() {   # keyed KEY: answers the next SDL X11 message box with one key, as a person who does not reach for the pointer
  # xdotool cannot read that window's UTF-8 name, xwininfo can.
  local w=""
  for _ in $(seq 1 120); do
    w=$(xwininfo -root -tree 2> /dev/null | awk '/"CNA Museum message"/ { print $1; exit }')
    [ -n "$w" ] && break
    kill -0 $museum 2> /dev/null || return
    sleep 0.5
  done
  [ -n "$w" ] || { say "no X11 message box"; shot missing-box; return; }
  local before; before=$(grep -ac 'MessageBox.Show' "$keylog")
  sleep 0.5
  xdotool windowfocus --sync "$w" key "$1"
  say "pressed $1 in the X11 message box"
  for _ in $(seq 1 20); do [ "$(grep -ac 'MessageBox.Show' "$keylog")" -gt "$before" ] && return; sleep 0.25; done
  say "X11 message box still open after $1: closing it"; shot stuck-box
  xdotool windowclose "$w" 2> /dev/null
}
box=0

answer "^Open File\$" "$dir/home/files/picked.png"
answer "^Save File\$" "$dir/home/files/saved.txt"
answer "Folder" "$dir/home/files/folder/"
w=$(window "^Open File\$")
if [ -n "$w" ]; then xdotool windowfocus --sync "$w" key Escape; gone "$w" && say "Open File cancelled with Escape"; fi
dismiss 0 simple
for i in 0 1 2; do dismiss $i show; done
wait $museum
echo $? > "$dir/status"
keylog=$dir/museum-keys.log
mkdir -p "$dir/no-zenity"
PATH=$dir/no-zenity LIBGL_ALWAYS_SOFTWARE=1 LP_NUM_THREADS=${LP_NUM_THREADS:-3} SDL_VIDEODRIVER=x11 \
  /usr/bin/timeout -k 10 "${MUSEUM_TIMEOUT:-300}" "$bin" --room dev-dialogs --no-audio --fixed-step 0.02 \
  --script "wait:30;press:MessageBox.Show (Yes;wait:10;press:MessageBox.Show (Yes;wait:10;stats;quit" > "$keylog" 2>&1 &
museum=$!
keyed Return
keyed Escape
wait $museum
echo $? > "$dir/status-keys"
INNER

passed=0 failed=0
ok() { echo "PASS  $1"; passed=$((passed + 1)); }
bad() { echo "FAIL  $1"; failed=$((failed + 1)); }
has() { grep -qF -- "$1" "$log"; }
line() { grep -aF -- "$1" "$log" | tail -1 | cut -c1-200; }
has "FileDialog.ShowOpenFile: 1 file(s) $dir/home/files/picked.png" && ok "ShowOpenFile through the portal returned the typed file" || bad "ShowOpenFile: $(line 'ShowOpenFile: ')"
has "FileDialog.ShowSaveFile: 1 file(s) $dir/home/files/saved.txt" && ok "ShowSaveFile through the portal returned the typed path" || bad "ShowSaveFile: $(line 'ShowSaveFile: ')"
has "FileDialog.ShowOpenFolder: 1 file(s) $dir/home/files/folder" && ok "ShowOpenFolder through the portal returned the typed folder" || bad "ShowOpenFolder: $(line 'ShowOpenFolder: ')"
has "FileDialog.ShowOpenFile: cancelled (no file)" && ok "Escape in the portal dialog reached the callback as a cancel" || bad "cancel: $(line 'ShowOpenFile: ')"
has "MessageBox.ShowSimple returned" && grep -q '^clicked button 0 .*(simple)' "$driver" && ! grep -q 'closing it (simple)' "$driver" && ok "MessageBox.ShowSimple closed by a pointer click on its button" || bad "ShowSimple: $(line 'ShowSimple')"
results=$(grep -aoE 'MessageBox.Show returned button -?[0-9]+ \([A-Za-z]+\)' "$log" | sed 's/MessageBox.Show returned button //' | tr '\n' ' ')
# The three clicks went to the buttons in reading order; each must report a different one of Yes, No and Cancel.
[ "$(grep -aoE 'MessageBox.Show returned button [0-2] ' "$log" | sort -u | wc -l)" = 3 ] && [ "$(grep -c '^clicked button .*(show)' "$driver")" = 3 ] && ok "MessageBox.Show reported each button clicked with the pointer: $results(in reading order)" || bad "MessageBox.Show: $results"
# CNA marks the first button the Return default and the last the Escape default (docs/devices-ext.md).
keys=$(grep -aoE 'MessageBox.Show returned button -?[0-9]+ ' "$dir/museum-keys.log" 2> /dev/null | sed 's/MessageBox.Show returned button //' | tr -d ' ' | tr '\n' ' ')
grep -q '^pressed Return' "$driver" && grep -q '^pressed Escape' "$driver" && [ "$keys" = "0 2 " ] && [ "$(cat "$dir/status-keys" 2> /dev/null)" = 0 ] &&
  ok "SDL's X11 message box: Return chose the first button, Escape the last" || bad "SDL's X11 message box by keyboard: Return, Escape gave ${keys:-nothing} (expected 0 2; exit $(cat "$dir/status-keys" 2> /dev/null))"
[ "$(cat "$dir/status" 2> /dev/null)" = 0 ] && ok "the museum exited cleanly" || bad "the museum exited with status $(cat "$dir/status" 2> /dev/null)"
grep -qE '\[museum\]\[stats\].*exhibit errors 0 .*script errors 0' "$log" && ok "final stats with 0 exhibit and 0 script errors" || bad "stats: $(grep -a '\[stats\]\|script-error' "$log" | tail -2 | tr '\n' ' ' | cut -c1-160)"
echo "summary: $passed passed, $failed failed (logs: $dir)"
python3 tools/record_profile.py dialogs --passed $passed --failed $failed || true
[ $failed = 0 ]
