#!/usr/bin/env bash
# Invitations, private slots, a party and signing out, between three museums signed in as the private gamer-services stack's
# three accounts (tools/online_stack.sh), each on its own private virtual display with its own profile, credential and data
# directories. The Guide is driven with xdotool on that display only (keys are held for a quarter second: outside the
# sign-in page the Guide reads which keys are down each frame, and a bare key tap can fall between two frames).
#
#   A, Visitor:   hosts a PlayerMatch session (1 local gamer, MaxGamers 3, PrivateGamerSlots 1), invites its friend Curator
#                 with Guide.ShowGameInvite, later invites Curator and Archivist into a party (Guide.ShowParty) and signs out.
#   C, Archivist: finds the session over the service (one public and one private slot open) and joins it: a public slot.
#   B, Curator:   accepts the game invitation on the card the Guide opens (InviteAccepted, JoinInvited: the private slot),
#                 then accepts the party invitation, as C does.
#
#   tools/online_party_check.sh      exit 0 only if every step passed; 77 when a prerequisite is absent (as online_check.sh)
#
# It runs its own stack in build/online-party on its own port (MUSEUM_ONLINE_PORT, default 47833), so it never touches an
# interactive stack or tools/online_check.sh's.
set -u
cd "$(dirname "$0")/.." || exit 2
for tool in Xvfb xdotool sqlite3; do command -v $tool > /dev/null || { echo "skipped: $tool is not installed"; exit 77; }; done
export MUSEUM_ONLINE_DIR=$PWD/${BUILD_DIR:-build}/online-party MUSEUM_ONLINE_PORT=${MUSEUM_ONLINE_PORT:-47833}
dir=$MUSEUM_ONLINE_DIR
bin=${MUSEUM_BIN:-${BUILD_DIR:-build}/bin/cna-3d-museum}
[ -x "$bin" ] || { echo "FAIL: $bin is not built"; exit 1; }
rm -rf "$dir/A" "$dir/B" "$dir/C"
mkdir -p "$dir"
declare -A display pid
xvfbs=()
cleanup() {
  for p in "${pid[@]}"; do kill "$p" 2> /dev/null; done
  for x in "${xvfbs[@]}"; do kill "$x" 2> /dev/null; done
  tools/online_stack.sh stop > /dev/null
}
trap cleanup EXIT
tools/online_stack.sh stop > /dev/null
tools/online_stack.sh start --fresh > "$dir/start.log" 2>&1
case $? in
  0) ;;
  3) echo "skipped: the service is not built (see tools/online_stack.sh)"; exit 77 ;;
  *) echo "FAIL: the service is built but did not start: $(tail -1 "$dir/start.log")"; exit 1 ;;
esac
eval "$(tools/online_stack.sh env)"
export SDL_AUDIODRIVER=dummy SDL_VIDEODRIVER=x11 LIBGL_ALWAYS_SOFTWARE=1 CNA_GAMER_SERVICES_REDUCED_MOTION=1
export LP_NUM_THREADS=${LP_NUM_THREADS:-${MUSEUM_RENDER_THREADS:-3}}
unset WAYLAND_DISPLAY   # SDL would prefer a live Wayland display; every museum here stays on its own Xvfb

passed=0 failed=0
ok() { echo "PASS  $1"; passed=$((passed + 1)); }
bad() { echo "FAIL  $1"; failed=$((failed + 1)); }
finish() {
  echo "summary: $passed passed, $failed failed (logs: $dir)"
  python3 tools/record_profile.py online-party --passed $passed --failed $failed ${1:+--note "$1"} || true
  [ $failed = 0 ] && exit 0 || exit 1
}
abort() { bad "$1"; finish "$1"; }

launch() {   # launch NAME SCRIPT: one museum on its own display, with its own profiles, credentials and data
  # Xvfb picks a free display itself (-displayfd): a guessed number could belong to another session's display.
  local n
  exec 3> "$dir/$1.display"
  Xvfb -displayfd 3 -nolisten tcp -screen 0 1280x720x24 > /dev/null 2>&1 &
  xvfbs+=($!)
  for _ in $(seq 1 50); do [ -s "$dir/$1.display" ] && break; sleep 0.1; done
  exec 3>&-
  n=$(tr -dc '0-9' < "$dir/$1.display")
  display[$1]=:$n
  mkdir -p "$dir/$1/data" "$dir/$1/profiles"
  mkdir -m 700 -p "$dir/$1/credentials"
  DISPLAY=:$n XDG_DATA_HOME="$dir/$1/data" CNA_GAMER_SERVICES_PROFILES_DIR="$dir/$1/profiles" CNA_GAMER_SERVICES_CREDENTIALS_DIR="$dir/$1/credentials" \
    timeout -k 15 "${MUSEUM_TIMEOUT:-600}" "$bin" --script "$2" --fixed-step 0.02 --no-audio > "$dir/$1.log" 2>&1 &
  pid[$1]=$!
}
count() { grep -cF -- "$2" "$dir/$1.log" 2> /dev/null || true; }
wait_for() {   # wait_for NAME TEXT [N]: until the museum NAME has logged TEXT at least N times (2 minutes at most)
  local need=${3:-1}
  for _ in $(seq 1 240); do
    [ "$(count "$1" "$2")" -ge "$need" ] && return 0
    kill -0 "${pid[$1]}" 2> /dev/null || return 1
    sleep 0.5
  done
  return 1
}
key() { DISPLAY=${display[$1]} xdotool keydown "$2"; sleep 0.25; DISPLAY=${display[$1]} xdotool keyup "$2"; sleep 0.6; }
sign_in() {   # sign_in NAME ACCOUNT PASSWORD: the Guide's sign-in page, as tools/online_check.sh types it
  wait_for "$1" "ShowSignIn(1, true) opened" || abort "$1: the sign-in page never opened"
  sleep 2
  DISPLAY=${display[$1]} xdotool windowfocus "$(DISPLAY=${display[$1]} xdotool search --onlyvisible --name '' | tail -1)" 2> /dev/null
  DISPLAY=${display[$1]} xdotool key Return; sleep 1.5
  DISPLAY=${display[$1]} xdotool type --delay 60 "$2"; DISPLAY=${display[$1]} xdotool key Return; sleep 4
  DISPLAY=${display[$1]} xdotool type --delay 60 "$3"; DISPLAY=${display[$1]} xdotool key Return
  wait_for "$1" "Guide.IsVisible false" || abort "$1: the Guide did not close after signing in $2"
}

signin_steps="goto:net-session;wait:30;press:Guide.ShowSignIn: an account for PlayerMatch;await:Guide.IsVisible false,6000;wait:30;press:Local gamers in a new session=1"
launch A "$signin_steps;press:MaxGamers in a new session=2;press:PrivateGamerSlots in a new session=1;press:Over the account service=1;press:Deliver packets=1;await:GamerJoined: Archivist,6000;press:Guide.ShowGameInvite;await:GamerJoined: Curator,9000;wait:50;goto:on-party;wait:60;press:Guide.ShowParty;await:PartySize of Visitor: 3,12000;goto:gs-players;wait:60;await:SignedInGamer.SignedOut,6000;wait:30;stats;quit"
sign_in A visitor museum-visitor-pass
wait_for A "Create(PlayerMatch" || abort "A: no PlayerMatch session was created: $(grep -a 'net\]' "$dir/A.log" | tail -1)"

launch C "$signin_steps;press:Over the account service=1;press:NetworkSession.BeginFind;await:session(s) found,3000;press:NetworkSession.BeginJoin;await:Joined:,3000;await:SessionEnded,12000;goto:on-party;wait:900;press:Guide.ShowParty;await:PartySize of Archivist: 3,9000;wait:30;stats;quit"
sign_in C archivist museum-archivist-pass
wait_for A "GamerJoined: Archivist" || abort "A: Archivist never joined: $(grep -a 'net\]' "$dir/C.log" | tail -2 | tr '\n' ' ')"

wait_for A "ShowGameInvite opened" || abort "A: Guide.ShowGameInvite did not open"
sleep 2.5
key A y   # the invitation page has A's friends ticked: Y sends and closes it

launch B "$signin_steps;await:InviteAccepted,6000;await:Joined:,3000;await:SessionEnded,12000;goto:on-party;wait:900;press:Guide.ShowParty;await:PartySize of Curator: 3,9000;wait:30;stats;quit"
sign_in B curator museum-curator-pass
# The invitation arrives within a few seconds of signing in and the Guide opens its card (Accept is focused).
wait_for B "Guide.IsVisible true" 2 || abort "B: no game invitation card opened"
sleep 2
key B Return
wait_for A "GamerJoined: Curator" || abort "A: Curator never joined: $(grep -a 'net\]' "$dir/B.log" | tail -2 | tr '\n' ' ')"
# Read now: the service deletes a session's invitations with the session, which ends when A leaves the room.
used=$(sqlite3 "$dir/service.sqlite3" "select count(*) from session_invitations i join users u on u.id=i.recipient_id where u.gamertag='Curator' and i.status='used'" 2> /dev/null)
# A party of three needs Archivist among Visitor's friends (the service invites mutual friends only). The shared fixture keeps
# Archivist a stranger for the Guide's friend pages, so this check's own service gets the friendship, and only now: earlier,
# Guide.ShowGameInvite would have ticked Archivist too and opened an invitation card on C.
python3 -I - "$dir/service.sqlite3" <<'PY' || { echo "FAIL: could not add the Visitor-Archivist friendship"; exit 1; }
import sqlite3, sys
db = sqlite3.connect(sys.argv[1])
ids = dict(db.execute("select username, id from users"))
db.execute("insert into friends(user_id, friend_id) values (?, ?)", (ids["visitor"], ids["archivist"]))
db.execute("insert into friends(user_id, friend_id) values (?, ?)", (ids["archivist"], ids["visitor"]))
db.commit()
PY

# The party: A opens its party page (no party yet, so the only row is "Invite friends") and invites both friends from the
# picker (Return invites the focused one), then closes the Guide.
wait_for A "ShowParty opened" || abort "A: Guide.ShowParty did not open"
sleep 3
key A Return    # Invite friends
sleep 2.5
key A Return    # the first friend
key A Down
key A Return    # the second friend
sleep 1.5
key A Escape; key A Escape
# B and C open their party pages after their waits: row 0 is the invitation from Visitor.
for n in B C; do
  wait_for $n "ShowParty opened" || abort "$n: Guide.ShowParty did not open"
  sleep 3
  key $n Return
  sleep 1.5
  key $n Escape
done
wait_for A "PartySize of Visitor: 3" || bad "A: the party never reached three members"

# Signing out: the Guide's Home page, whose last row (the sixth, for an account) is "Sign out".
wait_for A "goto gs-players" || abort "A: did not reach the players room"
sleep 2
key A Home
sleep 1.5
for _ in 1 2 3 4 5; do key A Down; done
key A Return

for n in A B C; do
  wait "${pid[$n]}"; status=$?
  unset "pid[$n]"
  [ $status = 0 ] && ok "$n: the museum exited cleanly" || bad "$n: the museum exited with status $status"
  grep -qE '\[museum\]\[stats\].*exhibit errors 0 .*script errors 0' "$dir/$n.log" && ok "$n: final stats with 0 exhibit and 0 script errors" ||
    bad "$n: stats line: $(grep -a '\[stats\]' "$dir/$n.log" | tail -1 | cut -c1-160)"
done
expect() {   # expect NAME 'regex' 'what it proves'
  grep -qE -- "$2" "$dir/$1.log" && ok "$3" || bad "$3 (no line in $1 matches: $2)"
}
expect A '\[museum\]\[net\] Create\(PlayerMatch, 1 local, 3 max, 1 private\)' "A created a PlayerMatch session with MaxGamers 3 and one private slot"
expect C '\[museum\]\[net\] found: host Visitor: 1 gamer\(s\), 1 public / 1 private slots open' "C found it through the service with one public and one private slot open"
expect A '\[museum\]\[net\] GamerJoined: Archivist \(remote, public slot\)' "C (Archivist) joined into the public slot"
expect A '\[museum\]\[online\] ShowGameInvite opened' "A opened Guide.ShowGameInvite for its friends"
expect B '\[museum\]\[net\] InviteAccepted: Curator, joining with JoinInvited' "B accepted the invitation on the Guide's card (NetworkSession.InviteAccepted)"
expect B '\[museum\]\[net\] Joined: 3 gamer\(s\)' "B joined with JoinInvited and sees three gamers"
expect A '\[museum\]\[net\] GamerJoined: Curator \(remote, private slot\)' "the invited gamer took the private slot"
expect A '\[museum\]\[online\] SignedInGamer.PartySize of Visitor: 3' "A's party has three members (PartySize)"
expect B '\[museum\]\[online\] SignedInGamer.PartySize of Curator: 3' "B is in A's party of three (PartySize)"
expect C '\[museum\]\[online\] SignedInGamer.PartySize of Archivist: 3' "C is in A's party of three (PartySize)"
expect A '\[museum\]\[players\] SignedInGamer.SignedOut: Visitor' "signing out in the Guide raised SignedInGamer.SignedOut"
[ "${used:-0}" = 1 ] && ok "the service marked Curator's invitation used while the session ran" || bad "the service held ${used:-no} used invitation(s) for Curator, expected 1"
members=$(sqlite3 "$dir/service.sqlite3" "select count(*) from party_members" 2> /dev/null)
[ "${members:-0}" = 3 ] && ok "the service holds a party of three" || bad "the service holds ${members:-no} party member(s), expected 3"
finish
