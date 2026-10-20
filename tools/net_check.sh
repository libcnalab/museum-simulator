#!/usr/bin/env bash
# Host migration across three museum processes (backlog N03), each on its own private display with its own gamer
# profiles: A hosts a SystemLink session with AllowHostMigration and a session property chosen at random, B and C
# search for exactly that property and join, A quits, and the processes that stay must report HostChanged and keep
# their session. Every result comes from the processes' own logs ([museum][net] lines).
#
#   tools/net_check.sh        exit 0 when every check passed (NET_ROUNDS="packets" runs only the packet round)
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
    CNA_GAMER_SERVICES_PROFILES_DIR="$dir/$1-profiles" XDG_DATA_HOME="$dir/$1-data" timeout -k 15 "${MUSEUM_TIMEOUT:-240}" \
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

run_packets() {   # A hosts with simulated loss and latency (both act on what A sends) and sends a burst with every SendDataOptions value; B judges it
  rm -rf "$dir"/*-profiles
  local host="goto:net-session;wait:20;press:Local gamers in a new session=1;press:Deliver packets=1;wait:20;press:SessionProperties[0] (game mode)=$mode;press:AllowJoinInProgress=1;press:SimulatedPacketLoss=0.3;press:SimulatedLatency (ms, on packets this museum sends)=100;press:Burst;wait:1100;press:NetworkSession.StartGame;wait:150;press:NetworkSession.EndGame;wait:200;stats;quit"
  local join="goto:net-session;wait:120;press:SessionProperties[0] (game mode)=$mode;press:Search for game mode=1;press:NetworkSession.BeginFind;wait:250;press:NetworkSession.BeginJoin;wait:1500;stats;quit"
  launch host "$host"
  sleep 4
  launch joinB "$join"
  for p in "${pids[@]}"; do wait "$p"; done
  pids=()
  for f in host joinB; do cp "$dir/$f.log" "$dir/$f-packets.log"; done
  local tag="[packets]" burst
  burst=$(grep -h "\[museum\]\[net\] burst received from" "$dir/joinB.log" | head -1 | sed 's/.*burst received from //')
  if [ -z "$burst" ]; then
    bad "$tag B received no burst: $(grep '\[museum\]\[net\]' "$dir/joinB.log" | tail -2 | cut -c1-160)"
  else
    echo "INFO  $tag $burst"
    count() { echo "$burst" | grep -oE "$1 [0-9]+/100 \([0-9]+ out of order, [0-9]+ duplicates\)" | head -1; }
    num() { echo "$1" | sed -E "s/.* ([0-9]+)\/100 \(([0-9]+) out of order, ([0-9]+) duplicates\)/\\$2/"; }
    local none reliable inorder rio
    none=$(count None); reliable=$(count Reliable); inorder=$(count InOrder); rio=$(count ReliableInOrder)
    grep -q "\[museum\]\[net\] burst sent: 4 x 100 packets .* SimulatedPacketLoss 0.30, SimulatedLatency 100 ms" "$dir/host.log" && ok "$tag A sent the burst with SimulatedPacketLoss 0.30 and SimulatedLatency 100 ms" || bad "$tag A: $(grep 'burst sent' "$dir/host.log" | head -1 | cut -c1-160)"
    [ "$(num "$reliable" 1)" = 100 ] && [ "$(num "$reliable" 3)" = 0 ] && ok "$tag Reliable: all 100 arrived despite the loss, no duplicates" || bad "$tag Reliable: $reliable"
    [ "$(num "$rio" 1)" = 100 ] && [ "$(num "$rio" 2)" = 0 ] && [ "$(num "$rio" 3)" = 0 ] && ok "$tag ReliableInOrder: all 100, in order, no duplicates" || bad "$tag ReliableInOrder: $rio"
    local n
    n=$(num "$inorder" 1)
    [ -n "$n" ] && [ "$n" -gt 0 ] && [ "$n" -lt 100 ] && [ "$(num "$inorder" 2)" = 0 ] && ok "$tag InOrder: $n of 100 arrived (lossy), never out of order" || bad "$tag InOrder: $inorder"
    n=$(num "$none" 1)
    [ -n "$n" ] && [ "$n" -ge 40 ] && [ "$n" -le 95 ] && ok "$tag None: $n of 100 arrived (a 30 % loss leaves about 70)" || bad "$tag None: $none"
  fi
  for f in host joinB; do
    grep -q "\[museum\]\[net\] GameStarted" "$dir/$f.log" && grep -q "\[museum\]\[net\] GameEnded" "$dir/$f.log" && ok "$tag $f saw GameStarted and GameEnded" || bad "$tag $f: no GameStarted/GameEnded"
    grep -q "\[museum\]\[stats\].* exhibit errors 0 .*script errors 0" "$dir/$f.log" || bad "$tag $f: errors: $(grep '\[museum\]\[stats\]\|exhibit-error\|script-error' "$dir/$f.log" | head -2 | cut -c1-160)"
  done
  grep -qE "\[museum\]\[net\] GamerJoined: .* \(remote\)" "$dir/host.log" && ok "$tag the host saw B's gamer join (GamerJoined, remote)" || bad "$tag no remote GamerJoined on the host"
}

run_lifecycle() {   # join in progress, removal by the host, and the removed museum joining again; each step awaits the last
  rm -rf "$dir"/*-profiles
  local host="goto:net-session;wait:20;press:Local gamers in a new session=1;press:Deliver packets=1;wait:20;press:SessionProperties[0] (game mode)=$mode;press:AllowJoinInProgress=1;wait:30;press:NetworkSession.StartGame;await:(remote),4000;wait:100;press:NetworkMachine.RemoveFromSession;await:GamerLeft,1000;await:(remote),4000;wait:100;stats;quit"
  local join="goto:net-session;wait:120;press:SessionProperties[0] (game mode)=$mode;press:Search for game mode=1;press:NetworkSession.BeginFind;wait:250;press:NetworkSession.BeginJoin;await:Joined: ,1500;await:SessionEnded: RemovedByHost,3000;wait:60;press:NetworkSession.BeginFind;wait:250;press:NetworkSession.BeginJoin;await:Joined: ,1500;wait:200;stats;quit"
  launch host "$host"
  sleep 4
  launch joinB "$join"
  for p in "${pids[@]}"; do wait "$p"; done
  pids=()
  for f in host joinB; do cp "$dir/$f.log" "$dir/$f-lifecycle.log"; done
  local tag="[lifecycle]"
  grep -qE "\[museum\]\[net\] Joined: .*AllowJoinInProgress true, state Playing" "$dir/joinB.log" && ok "$tag B joined a game in progress (state Playing, AllowJoinInProgress true)" || bad "$tag join in progress: $(grep 'net\] Joined' "$dir/joinB.log" | head -1 | cut -c1-200)"
  grep -q "\[museum\]\[net\] RemoveFromSession: removed" "$dir/host.log" && ok "$tag the host removed B's machine (NetworkMachine.RemoveFromSession)" || bad "$tag removal: $(grep 'RemoveFromSession' "$dir/host.log" | head -1)"
  grep -q "\[museum\]\[net\] SessionEnded: RemovedByHost" "$dir/joinB.log" && ok "$tag B's session ended with RemovedByHost" || bad "$tag B: $(grep 'SessionEnded' "$dir/joinB.log" | head -1)"
  grep -q "\[museum\]\[net\] GamerLeft" "$dir/host.log" && ok "$tag the host saw B's gamer leave (GamerLeft)" || bad "$tag no GamerLeft on the host"
  [ "$(grep -c '\[museum\]\[net\] Joined: ' "$dir/joinB.log")" -ge 2 ] && [ "$(grep -c '\[museum\]\[net\] GamerJoined: .* (remote)' "$dir/host.log")" -ge 2 ] && ok "$tag B joined again after the removal and the host saw it join twice" || bad "$tag reconnect: B joined $(grep -c 'net\] Joined: ' "$dir/joinB.log") time(s), host saw $(grep -c 'GamerJoined: .* (remote)' "$dir/host.log") remote join(s)"
  for f in host joinB; do
    grep -q "\[museum\]\[stats\].* exhibit errors 0 .*script errors 0" "$dir/$f.log" || bad "$tag $f: errors (an await that timed out is a script error): $(grep 'script-error\|exhibit-error' "$dir/$f.log" | head -2 | cut -c1-160)"
  done
}

pass=0 fail=0
ok() { echo "PASS  $1"; pass=$((pass + 1)); }
bad() { echo "FAIL  $1"; fail=$((fail + 1)); }
# One local gamer on the host first (the departing machine leaves nobody behind), then XNA's usual two.
# NET_ROUNDS picks rounds ("1 2 packets lifecycle" by default); the profile result is recorded only for the full set.
rounds=${NET_ROUNDS:-1 2 packets lifecycle}
for r in $rounds; do
  case $r in 1|2) run_round "$r" ;; packets) run_packets ;; lifecycle) run_lifecycle ;; *) echo "unknown round $r"; exit 2 ;; esac
done
echo "summary: $pass passed, $fail failed (logs: $dir)"
[ -z "${NET_ROUNDS:-}" ] && { python3 tools/record_profile.py net --passed $pass --failed $fail || true; }
[ $fail = 0 ]
