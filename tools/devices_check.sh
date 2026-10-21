#!/usr/bin/env bash
# Scripted check of the Devices and the System wing (floor 7): presses the manual buttons that a tour never
# presses and compares what the museum logs with what this environment can honestly deliver.
#
#   tools/devices_check.sh            exit 0 when nothing FAILed (UNAVAILABLE is reported, never counted as a pass)
#
# Isolation: a private Xvfb display; no session D-Bus (so no portal dialog, tray icon or browser reaches the real
# desktop); PATH starts with fake xdg-open and zenity programs that record their arguments and answer with fixed
# paths; HOME and XDG_CONFIG_HOME point at a copy of Content/MediaLibrary in build/devices-check, so MediaLibrary
# indexes only fixture files and SavePicture writes there. Message boxes are answered with xdotool (Return).
# Needs Xvfb and xdotool.
set -u
cd "$(dirname "$0")/.." || exit 2
for tool in Xvfb xdotool; do command -v $tool > /dev/null || { echo "skipped: $tool is not installed"; exit 77; }; done
bin=$PWD/${MUSEUM_BIN:-${BUILD_DIR:-build}/bin/cna-3d-museum}
[ -x "$bin" ] || { echo "FAIL: $bin is not built"; exit 1; }
dir=$PWD/${BUILD_DIR:-build}/devices-check
rm -rf "$dir"
mkdir -p "$dir/fakebin" "$dir/home"
cp -r Content/MediaLibrary/. "$dir/home/"
log=$dir/museum.log

cat > "$dir/fakebin/xdg-open" <<EOF
#!/bin/sh
printf '%s\n' "\$*" >> "$dir/xdg-open.log"
exit 0
EOF
cat > "$dir/fakebin/zenity" <<EOF
#!/bin/sh
printf '%s\n' "\$*" >> "$dir/zenity.log"
case "\$*" in
  --version) echo 4.0.1 ;;
  *--directory*) printf '%s\n' "$dir/home/Pictures" ;;
  *--save*) printf '%s\n' "$dir/home/notes.txt" ;;
  *--file-selection*) printf '%s\n' "$dir/home/Pictures/Exhibits/brick.png" ;;
  *) exit 1 ;;
esac
exit 0
EOF
chmod +x "$dir/fakebin/xdg-open" "$dir/fakebin/zenity"

display=$((350 + RANDOM % 40))
Xvfb :$display -screen 0 1920x1080x24 > /dev/null 2>&1 & xvfb=$!
museum= answerer=
cleanup() {
  [ -n "$answerer" ] && kill "$answerer" 2> /dev/null
  [ -n "$museum" ] && kill "$museum" 2> /dev/null
  kill "$xvfb" 2> /dev/null
}
trap cleanup EXIT
sleep 1
kill -0 $xvfb 2> /dev/null || { echo "FAIL: Xvfb :$display did not start"; exit 1; }

script="goto:dev-dialogs;wait:30;press:ShowOpenFile;wait:90;press:ShowSaveFile;wait:90;press:ShowOpenFolder;wait:90;press:MessageBox.ShowSimple;wait:30;press:MessageBox.Show (Yes;wait:30"
script+=";goto:dev-tray;wait:30;press:Create a tray icon;wait:10;press:Toggle the checkable entry;wait:5;press:Enable / disable;wait:5;press:UrlLauncher.Open;wait:30;press:Remove the tray icon;wait:5"
# A real camera is private: it is opened only with MUSEUM_ALLOW_CAMERA=1 (otherwise only the list is checked).
if [ "${MUSEUM_ALLOW_CAMERA:-0}" = 1 ]; then
  script+=";goto:dev-camera;wait:30;press:Open the default camera;wait:30;press:Close the camera;wait:5"
else
  script+=";goto:dev-camera;wait:30"
fi
script+=";goto:dev-haptics;wait:30;press:Open the first device;wait:5;press:Rumble;wait:5;press:Constant effect;wait:5;press:Gain 50;wait:5;press:Close the device;wait:5"
script+=";goto:dev-medialib;wait:30;press:Read my media library;wait:20;press:SavePicture;wait:10;press:Dispose the library;wait:5"
script+=";goto:dev-window;wait:30;press:MinimizeEXT;await:RestoreEXT returned,600;press:Emulate;wait:20"
script+=";goto:dev-identity;wait:20;pressall;wait:10"
script+=";goto:dev-notify;wait:20;pressall;wait:60;stats;quit"

env -u DBUS_SESSION_BUS_ADDRESS -u WAYLAND_DISPLAY DISPLAY=:$display PATH="$dir/fakebin:$PATH" HOME="$dir/home" XDG_CONFIG_HOME="$dir/home/.config" \
  SDL_VIDEODRIVER=x11 SDL_AUDIODRIVER=dummy SDL_FILE_DIALOG_DRIVER=zenity LIBGL_ALWAYS_SOFTWARE=1 LP_NUM_THREADS="${LP_NUM_THREADS:-3}" \
  CNA_GAMER_SERVICES_PROFILES_DIR="$dir/gamer-profiles" \
  timeout -k 10 "${MUSEUM_TIMEOUT:-300}" "$bin" --script "$script" --fixed-step 0.02 --no-audio > "$log" 2>&1 & museum=$!

# Modal message boxes block the game loop: answer whichever window appears with Return until the museum exits.
( while kill -0 $museum 2> /dev/null; do
    # The boxes are titled "CNA Museum message" (DeviceLab), never the game window; there is no window manager, so focus directly.
    for w in $(DISPLAY=:$display xdotool search --name "CNA Museum message" 2> /dev/null); do
      DISPLAY=:$display xdotool windowfocus "$w" key Return 2> /dev/null
    done
    sleep 2
  done ) & answerer=$!
wait $museum; status=$?; museum=

pass=0 fail=0 unavailable=0
ok() { echo "PASS  $1"; pass=$((pass + 1)); }
bad() { echo "FAIL  $1"; fail=$((fail + 1)); }
na() { echo "UNAVAILABLE  $1"; unavailable=$((unavailable + 1)); }
has() { grep -qF -- "$1" "$log"; }
line() { grep -F -- "$1" "$log" | head -1 | cut -c1-200; }

[ $status = 0 ] && ok "the museum ran the script and exited normally" || bad "the museum exited with status $status (log: $log)"
grep -q "\[museum\]\[stats\].* exhibit errors 0 .*script errors 0" "$log" && ok "no exhibit or script errors" || bad "errors: $(grep '\[museum\]\[stats\]\|exhibit-error\|script-error' "$log" | head -3)"

# Native dialogs: the fake zenity answers, the callback carries its answer back.
if has "FileDialog.ShowOpenFile refused"; then na "FileDialog: $(line 'FileDialog.ShowOpenFile refused')"
else
  has "FileDialog.ShowOpenFile: 1 file(s) $dir/home/Pictures/Exhibits/brick.png" && ok "FileDialog.ShowOpenFile returned the picked file through its callback" || bad "FileDialog.ShowOpenFile: $(line 'ShowOpenFile')"
  has "FileDialog.ShowSaveFile: 1 file(s) $dir/home/notes.txt" && ok "FileDialog.ShowSaveFile returned the save path" || bad "FileDialog.ShowSaveFile: $(line 'ShowSaveFile')"
  has "FileDialog.ShowOpenFolder: 1 file(s) $dir/home/Pictures" && ok "FileDialog.ShowOpenFolder returned the folder" || bad "FileDialog.ShowOpenFolder: $(line 'ShowOpenFolder')"
  grep -q -- "--file-selection" "$dir/zenity.log" 2> /dev/null && ok "the dialogs went to the fake zenity, not to a real desktop" || bad "zenity was never asked (log: $dir/zenity.log)"
fi
if has "MessageBox.ShowSimple refused"; then na "MessageBox: $(line 'MessageBox.ShowSimple refused')"
else
  has "MessageBox.ShowSimple returned" && ok "MessageBox.ShowSimple was shown and dismissed" || bad "MessageBox.ShowSimple: $(line 'ShowSimple')"
  # CNA marks no button as the Return or Escape default, so the Return key dismisses the box: the contract's -1.
  # Choosing a particular button needs a pointer click on it, which this check does not attempt.
  if has "MessageBox.Show returned button -1 (closed)"; then
    ok "MessageBox.Show reports a box dismissed without a choice as -1"
    na "MessageBox.Show button choice: needs a pointer click (no button is the Return default)"
  else
    grep -qE "MessageBox.Show returned button [0-2] " "$log" && ok "MessageBox.Show reported the button: $(grep -oE 'returned button [0-2] \([A-Za-z]+\)' "$log" | head -1)" || bad "MessageBox.Show: $(line 'MessageBox.Show ')"
  fi
fi

# Tray and URL: without a session bus there is no tray host; the URL goes to the fake xdg-open.
if has "this host has no system tray"; then na "SystemTray: no tray host on this display (no session D-Bus by design)"
elif has "tray: SystemTray refused:" && has "no SNI support"; then
  # SystemTray.IsSupported is the platform's static answer; the StatusNotifier host is only found when an icon is made.
  na "SystemTray: IsSupported is true, but creating the icon was refused without a session bus ($(line 'SystemTray refused' | sed 's/.*refused: //'))"
elif has "tray: icon shown with 2 entries"; then
  has "tray: checkable entry checked" && ok "SystemTray created, entry toggled" || bad "SystemTray toggle: $(line 'tray:')"
else bad "SystemTray: $(line 'tray:')"; fi
if has "UrlLauncher.Open(file://" && grep -q "Textures/checker.png" "$dir/xdg-open.log" 2> /dev/null; then ok "UrlLauncher.Open handed the local page to xdg-open: $(head -1 "$dir/xdg-open.log")"
elif has "UrlLauncher.Open refused"; then na "UrlLauncher: $(line 'UrlLauncher.Open refused')"
else bad "UrlLauncher: $(line 'UrlLauncher.Open') / xdg-open log: $(cat "$dir/xdg-open.log" 2> /dev/null)"; fi

# Camera and haptics: no hardware here; the refusal paths must say so, and nothing may pretend otherwise.
if grep -q "camera: supported .*, 0 camera(s)" "$log"; then
  na "Camera: no camera on this machine ($(line 'camera: supported'))"
elif grep -q "camera: supported yes, [1-9][0-9]* camera(s)" "$log"; then
  ok "Camera.AvailableCameras listed: $(line 'camera: supported' | sed 's/.*camera: //')"
  if [ "${MUSEUM_ALLOW_CAMERA:-0}" = 1 ]; then
    has "camera: camera opened" && ok "Camera opened: $(line 'camera: camera opened')" || bad "Camera open: $(grep 'camera: ' "$log" | tail -1)"
  else
    na "Camera open/frames: not attempted (a real camera is private; run with MUSEUM_ALLOW_CAMERA=1)"
  fi
else bad "Camera: $(line 'camera:')"; fi
if grep -q "haptics: 0 device(s)" "$log"; then
  na "Haptics: no haptic device on this machine"
  has "no haptic device; OpenEXT(unknown id) gave IsOpenEXT no" && ok "OpenEXT with no device returns a closed device" || bad "haptic refusal: $(line 'OpenEXT')"
  has "rumble refused: no open device" && has "effect refused: no open device" && has "gain refused: no open device" && ok "rumble, effects and gain are refused without a device" || bad "haptic refusals missing"
else
  has "haptics: OpenEXT: open yes" && ok "Haptic device opened: $(line 'OpenEXT: open')" || bad "Haptics: $(line 'haptics:')"
fi

# Media library: exact fixture contents.
if has "medialib] library: 3 / 2 / 2 / 2 songs/albums/artists/genres, 1 playlist(s), 4 / 2"; then ok "MediaLibrary indexed exactly the fixture: 3 songs, 2 albums, 2 artists, 2 genres, 1 playlist, 4 pictures in 2 albums"
else bad "MediaLibrary counts: $(line 'medialib] library:')"; fi
has "song #1 Sunrise - Museum Quartet, Opening Hours, Ambient, 1.2 s" && ok "song tags and duration read: Sunrise" || bad "song tags: $(line 'medialib: song')"
grep -q "medialib\] source .*, MediaSourceType LocalDevice" "$log" && ok "the default library's source is a LocalDevice: $(line 'MediaSourceType' | sed 's/.*medialib\] //')" || bad "media source: $(line 'medialib] source')"
has "playlist Favourites: Sunrise, The Archive" && ok "the .m3u playlist resolved to its two songs" || bad "playlist: $(line 'playlist ')"
if has "SavePicture: museum-swatch 2x2"; then
  find "$dir/home/Pictures" -newer "$dir/fakebin/zenity" -type f | grep -q . && ok "SavePicture wrote a 2x2 picture into the isolated Pictures folder" || bad "SavePicture reported success but no new file is under $dir/home/Pictures"
else bad "SavePicture: $(line 'SavePicture')"; fi
has "disposed: IsDisposed true" && ok "MediaLibrary.Dispose sets IsDisposed" || bad "Dispose: $(line 'disposed:')"

# Media library across changes, in a second museum process whose HOME starts empty: an empty library, then the fixture
# copied in, then one song removed and a picture album added, then SavePicture. Each change is made only after the
# museum logged the previous read (the script waits long enough; this loop waits for the log line), so no read races it.
media=$dir/media-home
mkdir -p "$media"
media_log=$dir/media.log
read_library="press:Read my media library;wait:400"
env -u DBUS_SESSION_BUS_ADDRESS -u WAYLAND_DISPLAY DISPLAY=:$display HOME="$media" XDG_CONFIG_HOME="$media/.config" \
  SDL_VIDEODRIVER=x11 SDL_AUDIODRIVER=dummy LIBGL_ALWAYS_SOFTWARE=1 LP_NUM_THREADS="${LP_NUM_THREADS:-3}" CNA_GAMER_SERVICES_PROFILES_DIR="$dir/gamer-profiles" \
  timeout -k 10 "${MUSEUM_TIMEOUT:-300}" "$bin" --script "goto:dev-medialib;wait:30;$read_library;$read_library;$read_library;press:SavePicture;wait:10;press:Read my media library;wait:20;stats;quit" \
  --fixed-step 0.02 --no-audio > "$media_log" 2>&1 & museum=$!
reads() { grep -c "medialib\] library:\|medialib\] MediaLibrary refused" "$media_log" 2> /dev/null; }
await_read() {   # await_read N: until the museum logged its Nth read (or exited)
  while [ "$(reads)" -lt "$1" ] && kill -0 $museum 2> /dev/null; do sleep 0.2; done
}
await_read 1
cp -r Content/MediaLibrary/. "$media/"
await_read 2
rm "$media/Music/03-the-archive.flac"
mkdir -p "$media/Pictures/Rooms"
cp Content/MediaLibrary/Pictures/Exhibits/brick.png "$media/Pictures/Rooms/lobby.png"
wait $museum; media_status=$?; museum=
mapfile -t libs < <(grep -o "medialib\] library: .*\|medialib\] MediaLibrary refused.*" "$media_log" | sed 's/medialib\] //')
[ $media_status = 0 ] && grep -q "\[museum\]\[stats\].* exhibit errors 0 .*script errors 0" "$media_log" && ok "second media process ran cleanly" || bad "media process: exit $media_status, $(grep 'stats\]\|exhibit-error' "$media_log" | head -2)"
[ "${libs[0]:-}" = "library: 0 / 0 / 0 / 0 songs/albums/artists/genres, 0 playlist(s), 0 / 0" ] && ok "an empty home gives an empty library: ${libs[0]}" || bad "empty library: ${libs[0]:-no read}"
[ "${libs[1]:-}" = "library: 3 / 2 / 2 / 2 songs/albums/artists/genres, 1 playlist(s), 4 / 2" ] && ok "after the fixture was copied in, a new read sees it all: ${libs[1]}" || bad "second read: ${libs[1]:-no read}"
[ "${libs[2]:-}" = "library: 2 / 1 / 1 / 1 songs/albums/artists/genres, 1 playlist(s), 5 / 3" ] && ok "a removed song and a new picture album show in the next read: ${libs[2]}" || bad "third read: ${libs[2]:-no read}"
grep -q "playlist Favourites: Sunrise (1.2 s)" "$media_log" && ok "the playlist entry whose file is gone is left out" || bad "playlist after removal: $(grep 'medialib\] playlist' "$media_log" | tail -1)"
[ "${libs[3]:-}" = "library: 2 / 1 / 1 / 1 songs/albums/artists/genres, 1 playlist(s), 6 / 4" ] && ok "a picture saved with SavePicture is in the next read: ${libs[3]}" || bad "read after SavePicture: ${libs[3]:-no read}"

# Window and identity.
has "window: MinimizeEXT returned; RestoreEXT follows in 2 s" && has "window: RestoreEXT returned" && ok "GameWindow.MinimizeEXT, then RestoreEXT 2 s later (deferred through the game, also while paused), both returned" || bad "minimise/restore: $(line 'window:')"
has "emulated: ContentLost raised 1 time(s), IsContentLost yes" && ok "emulated content loss raised ContentLost once and set IsContentLost" || bad "content loss: $(line 'emulated')"
has "after rebinding the target: IsContentLost no" && ok "binding the target again cleared IsContentLost" || bad "rebinding: $(line 'after rebinding')"
has "identity: platform Linux" && ok "identity: $(line 'identity: platform' | sed 's/.*identity: //')" || bad "identity: $(line 'identity:')"

# Notifications: loopback round trip.
grep -qE "notify\] open: ChannelUriUpdated 1 time\(s\), URI http://127\.0\.0\.1:[0-9]+/" "$log" && ok "the channel opened and announced a loopback URI" || bad "channel open: $(line 'notify] open')"
has "notify] received 13 byte(s): museum raw #1" && ok "the raw notification arrived on the game thread with its exact body" || bad "raw delivery: $(line 'notify] received')"
grep -q "notify\] received [0-9]* byte(s): <?xml.*<wp:Notification" "$log" && ok "the toast arrived as data (no shell displays it)" || bad "toast delivery: $(grep 'notify\] received' "$log" | tail -1)"
has "after Close: send refused, queued before Close and raised after it 2, URI null, Find null (no open channel of that name)" && ok "a closed channel refuses new messages and is no longer found; the two already queued are still raised" || bad "late send: $(line 'after Close')"
has "send to http://127.0.0.1:9/notify: refused" && ok "sending to an address nobody listens on is refused" || bad "nowhere: $(line '127.0.0.1:9')"

# Nothing may have escaped the private environment.
[ ! -e "$HOME/Pictures/museum-swatch.bmp" ] && ok "nothing was written into the real home folder" || bad "a picture appeared in the real $HOME/Pictures"

echo "summary: $pass passed, $fail failed, $unavailable unavailable (log: $log)"
python3 tools/record_profile.py devices --passed $pass --failed $fail --unavailable $unavailable || true
[ $fail = 0 ]
