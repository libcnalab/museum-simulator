#!/usr/bin/env bash
# The museum's window among other programs' windows, on a private virtual display with a real window manager (openbox from
# ~/deps/openbox) and a real XDND drag source (a small GTK window), driven with xdotool's pointer. Nothing touches the live
# desktop: Xvfb picks a free display, GTK is forced onto it and refuses to start anywhere else.
#   1. dev-window: text dragged from the GTK window onto the museum raises GameWindow.TextDropEXT with exactly that text.
#   2. pl-window: focus moves to the GTK window and back (Game.Deactivated, then Game.Activated), and a file dragged from it
#      raises GameWindow.FileDropEXT with exactly that path.
#   3. dev-window: GameWindow.MinimizeEXT leaves the window Iconic in the window manager's WM_STATE; RestoreEXT makes it Normal.
#   tools/window_check.sh      exit 0 when every check passed; 77 when a prerequisite is absent
set -u
cd "$(dirname "$0")/.." || exit 2
bin=${MUSEUM_BIN:-${BUILD_DIR:-build}/bin/cna-3d-museum}
ob=${MUSEUM_OPENBOX:-$HOME/deps/openbox}
[ -x "$bin" ] || { echo "FAIL: $bin is not built"; exit 1; }
for tool in Xvfb xdotool; do command -v $tool > /dev/null || { echo "skipped: $tool is not installed"; exit 77; }; done
[ -x "$ob/usr/bin/openbox" ] || { echo "skipped: openbox not found under $ob (MUSEUM_OPENBOX)"; exit 77; }
/usr/bin/python3 -c "import gi; gi.require_version('Gtk', '3.0')" 2> /dev/null || { echo "skipped: the system Python has no GTK 3 (python3-gi)"; exit 77; }

dir=${BUILD_DIR:-build}/window-check
mkdir -p "$dir"
text="Dropped from a private test window"
file=$PWD/Content/Textures/checker.png
passed=0 failed=0
ok() { echo "PASS  $1"; passed=$((passed + 1)); }
bad() { echo "FAIL  $1"; failed=$((failed + 1)); }
pids=()
cleanup() { for p in "${pids[@]}"; do kill "$p" 2> /dev/null; done; }
trap cleanup EXIT

# Xvfb picks a free display itself (-displayfd): a guessed number could belong to another session's display.
exec 3> "$dir/display"
Xvfb -displayfd 3 -screen 0 1920x1080x24 > /dev/null 2>&1 & pids+=($!)
for _ in $(seq 1 50); do [ -s "$dir/display" ] && break; sleep 0.1; done
exec 3>&-
n=$(tr -dc '0-9' < "$dir/display")
[ -n "$n" ] && kill -0 "${pids[0]}" 2> /dev/null || { echo "FAIL: Xvfb did not start"; exit 1; }
export DISPLAY=:$n
unset WAYLAND_DISPLAY
# Openbox finds its themes and default configuration in the unpacked package (without a theme it exits at once).
LD_LIBRARY_PATH="$ob/usr/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" XDG_DATA_DIRS="$ob/usr/share:/usr/share" \
  XDG_CONFIG_DIRS="$ob/etc/xdg" "$ob/usr/bin/openbox" > "$dir/openbox.log" 2>&1 & pids+=($!)
sleep 1
kill -0 "${pids[1]}" 2> /dev/null || { echo "FAIL: openbox did not stay up: $(tail -1 "$dir/openbox.log")"; exit 1; }

source_window() {   # source_window text|uri VALUE: the GTK drag source at the right of the screen, offering VALUE (pid in source_pid)
  # GDK_BACKEND=x11: with WAYLAND_DISPLAY unset, GTK would otherwise reach the live desktop's default Wayland socket.
  GDK_BACKEND=x11 /usr/bin/python3 -I - "$1" "$2" > "$dir/source-$1.log" 2>&1 <<'PY' &
import sys, gi
gi.require_version("Gtk", "3.0")
from gi.repository import Gtk, Gdk
kind, value = sys.argv[1], sys.argv[2]
# Refuse to open anything unless GTK is on the private X display it was given (never the live desktop).
assert type(Gdk.Display.get_default()).__name__.startswith("X11"), "GTK is not on the private X display"
window = Gtk.Window(title="museum-drag-source")
window.set_default_size(240, 120)
window.move(1600, 100)
button = Gtk.Button(label="drag me")   # a button reliably starts a drag from synthetic pointer events
button.drag_source_set(Gdk.ModifierType.BUTTON1_MASK, [], Gdk.DragAction.COPY)
if kind == "uri":
    button.drag_source_add_uri_targets()
else:
    button.drag_source_add_text_targets()
def give(widget, context, data, info, time):
    print("drag-data-get for", data.get_target().name(), flush=True)
    if kind == "uri":
        data.set_uris(["file://" + value])
    else:
        data.set_text(value, -1)
button.connect("drag-data-get", give)
button.connect("drag-begin", lambda *a: print("drag-begin", flush=True))
button.connect("drag-end", lambda *a: print("drag-end", flush=True))
window.add(button)
window.connect("destroy", Gtk.main_quit)
window.show_all()
print("source ready on", Gdk.Display.get_default().get_name(), flush=True)
Gtk.main()
PY
  source_pid=$!
  pids+=($source_pid)
}
museum() {   # museum ROOM SCRIPT LOG: one museum run on the private display (pid in museum_pid; started here, so wait works)
  env LIBGL_ALWAYS_SOFTWARE=1 LP_NUM_THREADS=${LP_NUM_THREADS:-3} SDL_VIDEODRIVER=x11 XDG_DATA_HOME="$dir/data" \
    timeout -k 15 "${MUSEUM_TIMEOUT:-240}" "$bin" --room "$1" --script "$2" --fixed-step 0.02 --no-audio > "$3" 2>&1 &
  museum_pid=$!
  pids+=($museum_pid)
}
window_id() { xdotool search --onlyvisible --name "$1" 2> /dev/null | head -1; }
geometry() {   # geometry ID: "x y w h"
  xdotool getwindowgeometry --shell "$1" | awk -F= '/^X=/{x=$2} /^Y=/{y=$2} /^WIDTH=/{w=$2} /^HEIGHT=/{h=$2} END {print x, y, w, h}'
}
wait_windows() {   # wait_windows: sets src and dst to the two window ids (60 s at most)
  src="" dst=""
  for _ in $(seq 1 60); do
    src=$(window_id museum-drag-source); dst=$(window_id "CNA 3D Museum")
    [ -n "$src" ] && [ -n "$dst" ] && return 0
    sleep 1
  done
  return 1
}
drag() {   # drag SRC DST: the pointer presses on the source, moves past GTK's threshold, crosses to the museum and releases
  local sx sy sw sh dx dy dw dh i
  read -r sx sy sw sh <<< "$(geometry "$1")"
  read -r dx dy dw dh <<< "$(geometry "$2")"
  xdotool mousemove $((sx + sw / 2)) $((sy + sh / 2)); sleep 0.3
  xdotool mousedown 1; sleep 0.3
  for i in $(seq 1 10); do xdotool mousemove_relative -- -5 2; sleep 0.1; done
  for i in $(seq 1 30); do
    xdotool mousemove $((sx + sw / 2 + (dx + dw / 2 - sx - sw / 2) * i / 30)) $((sy + sh / 2 + (dy + dh / 2 - sy - sh / 2) * i / 30)); sleep 0.1
  done
  sleep 0.5
  xdotool mouseup 1
}
finish_run() {   # finish_run PID LOG NAME: the museum's exit status and its final stats line
  wait "$1"; local status=$?
  [ $status = 0 ] && ok "$3: the museum exited cleanly" || bad "$3: the museum exited with status $status"
  grep -qE '\[museum\]\[stats\].*exhibit errors 0 .*script errors 0' "$2" && ok "$3: final stats with 0 exhibit and 0 script errors" ||
    bad "$3: stats line: $(grep -a '\[stats\]\|script-error' "$2" | tail -2 | tr '\n' ' ' | cut -c1-200)"
}

# 1. Text onto dev-window. (directory:open puts a menu up, which frees the pointer the museum otherwise captures.)
source_window text "$text"; s=$source_pid
log=$dir/museum-text.log
museum dev-window "wait:60;directory:open;await:TextDropEXT,6000;wait:20;directory:close;stats;quit" "$log"; m=$museum_pid
if wait_windows; then
  ok "text: the GTK drag source and the museum window are on the private display :$n"
  sleep 3
  drag "$src" "$dst"
else
  bad "text: the windows did not appear (source $src, museum $dst)"
fi
finish_run "$m" "$log" text
grep -qF "[museum][devices] TextDropEXT: \"$text\"" "$log" && ok "GameWindow.TextDropEXT delivered the dragged text exactly: \"$text\"" ||
  bad "no TextDropEXT with the dragged text: $(grep -a 'TextDropEXT' "$log" | head -1 | cut -c1-160)"
kill "$s" 2> /dev/null; sleep 1

# 2. Focus away and back, then a file onto pl-window.
source_window uri "$file"; s=$source_pid
log=$dir/museum-focus.log
# The museum only waits for the file; each focus change is counted by the driver around its own action (the window manager
# may move focus once on its own while the windows appear).
museum pl-window "wait:60;directory:open;await:FileDropEXT,9000;wait:20;directory:close;stats;quit" "$log"; m=$museum_pid
if wait_windows; then
  sleep 3
  count() { grep -c "\[museum\]\[window\] Game.$1 " "$log" || true; }
  xdotool windowactivate --sync "$dst" 2> /dev/null; sleep 2   # start from the museum in focus
  before=$(count Deactivated)
  xdotool windowactivate --sync "$src" 2> /dev/null; sleep 2
  [ "$(count Deactivated)" -gt "$before" ] && ok "activating the other program's window raised Game.Deactivated" || bad "no Game.Deactivated when the other window was activated"
  before=$(count Activated)
  xdotool windowactivate --sync "$dst" 2> /dev/null; sleep 2
  [ "$(count Activated)" -gt "$before" ] && ok "activating the museum again raised Game.Activated" || bad "no Game.Activated when the museum was activated again"
  drag "$src" "$dst"
else
  bad "focus: the windows did not appear (source $src, museum $dst)"
fi
finish_run "$m" "$log" focus
grep -qF "[museum][window] FileDropEXT: 1 file(s): $file" "$log" && ok "GameWindow.FileDropEXT delivered the dragged file exactly: $file" ||
  bad "no FileDropEXT with the dragged file: $(grep -a 'FileDropEXT' "$log" | head -1 | cut -c1-160)"

kill "$s" 2> /dev/null; sleep 1

# 3. GameWindow.MinimizeEXT, then RestoreEXT two seconds later (dev-window): the window manager's own WM_STATE in between.
log=$dir/museum-minimise.log
museum dev-window "wait:60;press:GameWindow.MinimizeEXT;await:RestoreEXT returned,1500;wait:30;stats;quit" "$log"; m=$museum_pid
state() { xprop -id "$1" WM_STATE 2> /dev/null | sed -n 's/.*window state: //p' | head -1; }
dst=""
for _ in $(seq 1 60); do dst=$(xdotool search --name "CNA 3D Museum" 2> /dev/null | head -1); [ -n "$dst" ] && break; sleep 1; done
minimised="" restored=""
for _ in $(seq 1 120); do grep -q 'window: MinimizeEXT returned' "$log" 2> /dev/null && break; sleep 0.25; done
sleep 0.5; [ -n "$dst" ] && minimised=$(state "$dst")
for _ in $(seq 1 120); do grep -q 'window: RestoreEXT returned' "$log" 2> /dev/null && break; sleep 0.25; done
sleep 0.5; [ -n "$dst" ] && restored=$(state "$dst")
finish_run "$m" "$log" minimise
[ "$minimised" = Iconic ] && ok "after MinimizeEXT the window manager holds the window Iconic" || bad "after MinimizeEXT the window state is '${minimised:-unknown}', expected Iconic"
[ "$restored" = Normal ] && ok "after RestoreEXT the window is Normal again" || bad "after RestoreEXT the window state is '${restored:-unknown}', expected Normal"

echo "summary: $passed passed, $failed failed (logs: $dir)"
python3 tools/record_profile.py window --passed $passed --failed $failed || true
[ $failed = 0 ]
