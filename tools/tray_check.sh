#!/usr/bin/env bash
# The tray icon of dev-tray (SystemTray over SDL's StatusNotifierItem) against a private tray host: a private session bus
# (dbus-run-session) carries a small StatusNotifierWatcher and host written with GDBus, the museum runs on a private Xvfb, and
# the host reads the item's tooltip and menu back and clicks "Say hello" exactly as a desktop panel does (dbusmenu Event).
# The user's desktop, session bus and panel are never involved.
#   tools/tray_check.sh      exit 0 when every check passed; 77 when a prerequisite is absent
set -u
cd "$(dirname "$0")/.." || exit 2
bin=${MUSEUM_BIN:-${BUILD_DIR:-build}/bin/cna-3d-museum}
[ -x "$bin" ] || { echo "FAIL: $bin is not built"; exit 1; }
for tool in Xvfb dbus-run-session; do command -v $tool > /dev/null || { echo "skipped: $tool is not installed"; exit 77; }; done
/usr/bin/python3 -c "import gi; gi.require_version('Gio', '2.0')" 2> /dev/null || { echo "skipped: the system Python has no GObject introspection"; exit 77; }
dir=${BUILD_DIR:-build}/tray-check
mkdir -p "$dir"
log=$dir/museum.log host=$dir/host.log

# Everything below runs inside its own session bus, which ends with it.
env -u DBUS_SESSION_BUS_ADDRESS dbus-run-session -- bash -s "$bin" "$dir" "$log" "$host" <<'INNER'
bin=$1 dir=$2 log=$3 host=$4
case "$DBUS_SESSION_BUS_ADDRESS" in *"$XDG_RUNTIME_DIR/bus"*) echo "FAIL: not a private session bus"; exit 1 ;; esac
/usr/bin/python3 -I - > "$host" 2>&1 <<'PY' &
from gi.repository import Gio, GLib
XML = """<node><interface name="org.kde.StatusNotifierWatcher">
  <method name="RegisterStatusNotifierItem"><arg type="s" direction="in"/></method>
  <method name="RegisterStatusNotifierHost"><arg type="s" direction="in"/></method>
  <property name="RegisteredStatusNotifierItems" type="as" access="read"/>
  <property name="IsStatusNotifierHostRegistered" type="b" access="read"/>
  <property name="ProtocolVersion" type="i" access="read"/>
</interface></node>"""
items = []
def say(*parts): print(*parts, flush=True)
def click(bus, service):
    def prop(path, interface, name):
        return bus.call_sync(service, path, "org.freedesktop.DBus.Properties", "Get", GLib.Variant("(ss)", (interface, name)),
                             None, Gio.DBusCallFlags.NONE, 3000, None).unpack()[0]
    try:
        # Plain properties one at a time first, so a crash in the item names the property that caused it.
        for name in ("Id", "Category", "Status", "Title", "IconName"):
            say("reading", name)
            say(" ", name, "=", repr(prop("/StatusNotifierItem", "org.kde.StatusNotifierItem", name)))
        menu = prop("/StatusNotifierItem", "org.kde.StatusNotifierItem", "Menu")
        layout = bus.call_sync(service, menu, "com.canonical.dbusmenu", "GetLayout", GLib.Variant("(iias)", (0, -1, [])),
                               None, Gio.DBusCallFlags.NONE, 3000, None).unpack()[1]
        labels = {child[0]: child[1].get("label", "") for child in layout[2]}
        say("menu:", "; ".join(f"{k}={v}" for k, v in sorted(labels.items())))
        target = next(k for k, v in labels.items() if v == "Say hello")
        bus.call_sync(service, menu, "com.canonical.dbusmenu", "Event", GLib.Variant("(isvu)", (target, "clicked", GLib.Variant("s", ""), 0)),
                      None, Gio.DBusCallFlags.NONE, 3000, None)
        say("clicked:", target)
        # The tooltip last: reading it has made the item's process abort (docs/CNA_FINDINGS.md), so it must not hide the rest.
        GLib.timeout_add(4000, tooltip, bus, service, prop)
    except Exception as error:
        say("host error:", error)
    return False
def tooltip(bus, service, prop):
    try:
        say("reading ToolTip")
        value = prop("/StatusNotifierItem", "org.kde.StatusNotifierItem", "ToolTip")
        say("tooltip:", value[3] if len(value) > 3 else value)
    except Exception as error:
        say("tooltip error:", error)
    return False
def method(connection, sender, path, interface, name, parameters, invocation):
    if name == "RegisterStatusNotifierItem":
        service = parameters.unpack()[0]
        if not service.startswith(":") and not service.startswith("org."):
            service = sender
        items.append(service)
        say("registered:", service)
        GLib.timeout_add(1000, click, connection, service)
    invocation.return_value(None)
def get(connection, sender, path, interface, name):
    return {"RegisteredStatusNotifierItems": GLib.Variant("as", items), "IsStatusNotifierHostRegistered": GLib.Variant("b", True),
            "ProtocolVersion": GLib.Variant("i", 0)}[name]
def acquired(connection, name):
    connection.register_object("/StatusNotifierWatcher", Gio.DBusNodeInfo.new_for_xml(XML).interfaces[0], method, get, None)
    say("watcher ready")
Gio.bus_own_name(Gio.BusType.SESSION, "org.kde.StatusNotifierWatcher", Gio.BusNameOwnerFlags.NONE, acquired, None, None)
GLib.MainLoop().run()
PY
hostpid=$!
for _ in $(seq 1 50); do grep -q 'watcher ready' "$host" 2> /dev/null && break; sleep 0.1; done
exec 3> "$dir/display"
Xvfb -displayfd 3 -screen 0 1280x720x24 > /dev/null 2>&1 & xvfb=$!
for _ in $(seq 1 50); do [ -s "$dir/display" ] && break; sleep 0.1; done
exec 3>&-
env -u WAYLAND_DISPLAY DISPLAY=:$(tr -dc '0-9' < "$dir/display") SDL_VIDEODRIVER=x11 LIBGL_ALWAYS_SOFTWARE=1 LP_NUM_THREADS=${LP_NUM_THREADS:-3} \
  XDG_DATA_HOME="$dir/data" timeout -k 15 "${MUSEUM_TIMEOUT:-240}" "$bin" --room dev-tray --no-audio --fixed-step 0.02 --script \
  "wait:60;press:Create a tray icon;await:tray entry clicked,3000;press:Toggle the checkable entry;wait:10;press:Enable / disable the first entry;wait:300;press:Remove the tray icon;wait:20;stats;quit" \
  > "$log" 2>&1
echo $? > "$dir/status"
kill $hostpid $xvfb 2> /dev/null
INNER

passed=0 failed=0
ok() { echo "PASS  $1"; passed=$((passed + 1)); }
bad() { echo "FAIL  $1"; failed=$((failed + 1)); }
check() { grep -qF -- "$2" "$3" && ok "$1" || bad "$1 (no line with: $2)"; }
check "SystemTray created the icon with two entries" "[museum][devices] tray: icon shown with 2 entries" "$log"
check "the item registered with the private StatusNotifierWatcher" "registered: " "$host"
notrun=0
if grep -q '_dbus_check_is_valid_utf8' "$log"; then
  # A known CNA defect ends the run at the first menu request; what depends on the menu is not verified, not passed.
  bad "a tray host reading the menu made the museum abort (dangling entry labels, docs/CNA_FINDINGS.md)"
  notrun=7
  echo "summary: $passed passed, $failed failed, $notrun not run because of it (logs: $dir)"
  python3 tools/record_profile.py tray --passed $passed --failed $failed --not-run $notrun --note "CNA: a tray host's menu request aborts the game (dangling labels)" || true
  exit 1
fi
grep -qE '^menu: .*Say hello.*Night mode' "$host" && ok "the host read both entries from the item's dbusmenu" || bad "menu read back: $(grep '^menu:' "$host")"
check "a dbusmenu click on \"Say hello\" ran the museum's entry callback" "[museum][devices] tray entry clicked" "$log"
check "SetEntryChecked/SetEntryLabel on the checkable entry" "tray: checkable entry checked, clicks 1" "$log"
check "SetEntryEnabled on the first entry" "tray: first entry disabled" "$log"
check "the icon was removed" "tray: icon removed" "$log"
[ "$(cat "$dir/status" 2> /dev/null)" = 0 ] && ok "the museum exited cleanly" || bad "the museum exited with status $(cat "$dir/status" 2> /dev/null)"
check "the host read the tooltip back over D-Bus (the museum survives it)" "tooltip: CNA 3D Museum: Devices and the System" "$host"
grep -qE '\[museum\]\[stats\].*exhibit errors 0 .*script errors 0' "$log" && ok "final stats with 0 exhibit and 0 script errors" || bad "stats: $(grep -a '\[stats\]\|script-error' "$log" | tail -2 | tr '\n' ' ' | cut -c1-160)"
echo "summary: $passed passed, $failed failed (logs: $dir)"
python3 tools/record_profile.py tray --passed $passed --failed $failed || true
[ $failed = 0 ]
