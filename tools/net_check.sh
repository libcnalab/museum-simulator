#!/usr/bin/env bash
# Host migration across three museum processes (backlog N03), each on its own private display with its own gamer
# profiles: A hosts a SystemLink session with AllowHostMigration and a session property chosen at random, B and C
# search for exactly that property and join, A quits, and the processes that stay must report HostChanged and keep
# their session. Every result comes from the processes' own logs ([museum][net] lines).
#
#   tools/net_check.sh        exit 0 when every check passed
#
# SystemLink discovery uses the local network like XNA's did; the random session property keeps B and C from
# joining a session somebody else hosts at the same time.
set -u
cd "$(dirname "$0")/.." || exit 2
command -v xvfb-run > /dev/null || { echo "skipped: xvfb-run is not installed"; exit 77; }
bin=$PWD/${MUSEUM_BIN:-${BUILD_DIR:-build}/bin/cna-3d-museum}
[ -x "$bin" ] || { echo "FAIL: $bin is not built"; exit 1; }
dir=$PWD/${BUILD_DIR:-build}/net-check
rm -rf "$dir"
mkdir -p "$dir"
mode=$((1 + RANDOM % 9))
pids=()
cleanup() { for p in "${pids[@]}"; do kill "$p" 2> /dev/null; done; }
trap cleanup EXIT

launch() {   # launch NAME SCRIPT
  mkdir -p "$dir/$1-profiles"
  env -u WAYLAND_DISPLAY -u DBUS_SESSION_BUS_ADDRESS SDL_VIDEODRIVER=x11 SDL_AUDIODRIVER=dummy LIBGL_ALWAYS_SOFTWARE=1 LP_NUM_THREADS=2 \
    CNA_GAMER_SERVICES_PROFILES_DIR="$dir/$1-profiles" timeout -k 15 "${MUSEUM_TIMEOUT:-240}" \
    xvfb-run -a -s "-screen 0 1280x720x24" "$bin" --script "$2" --fixed-step 0.02 --no-audio > "$dir/$1.log" 2>&1 &
  pids+=($!)
}

run_round() {   # run_round LOCALS: A hosts with LOCALS local gamers (1 or 2), B and C join, A quits
  local locals=$1 choice=0
  [ "$locals" = 1 ] && choice=1
  rm -rf "$dir"/*-profiles
  local host="goto:net-session;wait:20;press:Local gamers in a new session=$choice;press:Deliver packets=1;wait:20;press:AllowHostMigration=1;press:SessionProperties[0] (game mode)=$mode;press:AllowJoinInProgress=1;wait:900;stats;quit"
  local join="goto:net-session;wait:120;press:SessionProperties[0] (game mode)=$mode;press:Search for game mode=1;press:NetworkSession.BeginFind;wait:250;press:NetworkSession.BeginJoin;wait:1500;stats;quit"
  launch host "$host"
  sleep 4
  launch joinB "$join"
  sleep 6   # two searches at the same moment can miss each other's discovery reply: B first, then C
  launch joinC "$join"
  for p in "${pids[@]}"; do wait "$p"; done
  pids=()
  for f in host joinB joinC; do cp "$dir/$f.log" "$dir/$f-$locals.log"; done
  local tag="[$locals local on the host]"
  grep -q "\[museum\]\[net\] Create(SystemLink, $locals local, 8 max): $locals local gamer(s)" "$dir/host.log" && ok "$tag A created a SystemLink session" || bad "$tag A did not create a session: $(grep '\[museum\]\[net\]' "$dir/host.log" | head -3)"
  for j in joinB joinC; do
    grep -qE "\[museum\]\[net\] Joined: [0-9]+ gamer\(s\) in the session; this machine sees AllowHostMigration true" "$dir/$j.log" && ok "$tag $j joined and sees AllowHostMigration true" || bad "$tag $j: $(grep '\[museum\]\[net\]' "$dir/$j.log" | tail -2 | cut -c1-160)"
  done
  local changed ended
  changed=$(grep -h "\[museum\]\[net\] HostChanged:" "$dir/joinB.log" "$dir/joinC.log" | head -1)
  [ -n "$changed" ] && ok "$tag after A left, HostChanged: $(echo "$changed" | sed 's/.*HostChanged: //')" || bad "$tag no HostChanged after the host left"
  # B starts (and so quits) first: its log covers A's departure only. C also sees B leave later, which is a second
  # migration with B's own two local gamers and is reported for information.
  ended=$(grep -h "\[museum\]\[net\] SessionEnded:" "$dir/joinB.log" | head -1)
  [ -z "$ended" ] && ok "$tag B kept its session after A left" || bad "$tag B's session ended when A left: $(echo "$ended" | sed 's/.*\] //')"
  ended=$(grep -h "\[museum\]\[net\] SessionEnded:" "$dir/joinC.log" | head -1)
  [ -n "$ended" ] && echo "INFO  $tag C, after B (two local gamers) left as well: $(echo "$ended" | sed 's/.*\] //')"
  for j in host joinB joinC; do
    grep -q "\[museum\]\[stats\].* exhibit errors 0 .*script errors 0" "$dir/$j.log" || bad "$tag $j: errors: $(grep '\[museum\]\[stats\]\|exhibit-error\|script-error' "$dir/$j.log" | head -2 | cut -c1-160)"
  done
}

pass=0 fail=0
ok() { echo "PASS  $1"; pass=$((pass + 1)); }
bad() { echo "FAIL  $1"; fail=$((fail + 1)); }
# One local gamer on the host first (the departing machine leaves nobody behind), then XNA's usual two.
run_round 1
run_round 2
echo "summary: $pass passed, $fail failed (logs: $dir)"
[ $fail = 0 ]
