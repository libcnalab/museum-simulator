#!/usr/bin/env bash
# A private CNA gamer-services server for the Online rooms: accounts, friends, messages, parties, leaderboards and the session
# directory need a service; without one the museum runs offline and those rooms say so. This script starts the sibling
# cna-gamer-services-server on a loopback port with a throw-away database in the build tree, with three accounts and the museum's
# achievements, a friendship (visitor and curator) and a message, and prints the environment the museum needs.
#
#   tools/online_stack.sh start [--fresh]    build the database on first use (--fresh: from scratch) and start the server
#   tools/online_stack.sh stop | status
#   eval "$(tools/online_stack.sh env)"      then run the museum: its Guide signs in the accounts below
#
# Exit status of start: 0 running, 3 the service is not built (nothing was attempted), 1 it is built but failed to provision or
# to open its port (a real failure, see the server log). MUSEUM_ONLINE_DIR (default build/online) and MUSEUM_ONLINE_PORT keep a
# second stack (tools/online_check.sh uses its own) from sharing the database, pid file or port of an interactive one.
#
# Accounts (a local test fixture, loopback only): visitor / museum-visitor-pass, curator / museum-curator-pass,
# archivist / museum-archivist-pass. The server must be built once:
#   (cd ../cna-gamer-services-server && cmake --preset dev && cmake --build --preset dev)
set -u
cd "$(dirname "$0")/.." || exit 2
server_dir=${MUSEUM_SERVICE_DIR:-../cna-gamer-services-server}
bin=${MUSEUM_SERVICE_BUILD:-$server_dir/build-agent}
dir=${MUSEUM_ONLINE_DIR:-${BUILD_DIR:-build}/online}
case $dir in /*) ;; *) dir=$PWD/$dir ;; esac
port=${MUSEUM_ONLINE_PORT:-47831}
title=cna-3d-museum
db=$dir/service.sqlite3
pidfile=$dir/server.pid

running() { [ -f "$pidfile" ] && kill -0 "$(cat "$pidfile")" 2>/dev/null; }

provision() {
  rm -f "$db" "$db"-wal "$db"-shm
  "$bin/cna-gamer-services-admin" "$db" title "$title" "CNA 3D Museum" > /dev/null || return 1
  local u
  for u in visitor:Visitor curator:Curator archivist:Archivist; do
    printf '%s' "museum-${u%%:*}-pass" | "$bin/cna-gamer-services-admin" "$db" user "${u%%:*}" "${u##*:}" > /dev/null || return 1
  done
  python3 -I - "$bin" "$db" "$title" <<'PY'
import json, subprocess, sys
admin, db, title = sys.argv[1] + "/cna-gamer-services-admin", sys.argv[2], sys.argv[3]
for a in json.load(open("Content/GamerServices/Achievements.json")):
    a.setdefault("display", True)
    subprocess.run([admin, db, "achievement", title], input=json.dumps(a).encode(), check=True, stdout=subprocess.DEVNULL)
PY
  # The museum's score board (what LeaderboardKey.BestScoreLifeTime reads, game mode 0): higher is better, the best rating is kept.
  python3 -I - "$bin" "$db" "$title" <<'PY'
import json, subprocess, sys
admin, db, title = sys.argv[1] + "/cna-gamer-services-admin", sys.argv[2], sys.argv[3]
def run(cmd, doc):
    subprocess.run([admin, db, cmd, title], input=json.dumps(doc).encode(), check=True, stdout=subprocess.DEVNULL)
run("leaderboard", {"key": "BestScoreLifeTime", "mode": 0, "ascending": False, "aggregation": "best", "arbitrated": False,
                    "columns": {"MuseumRuns": "int32", "Outcome": "outcome", "Exhibit": "string"}})
for tag, rating in (("Curator", 120), ("Archivist", 80)):
    run("seed-leaderboard", {"key": "BestScoreLifeTime", "mode": 0, "gamertag": tag, "rating": rating, "columns": {}})
PY
  # Visitor and Curator start as friends and Curator has left a message; Archivist is a stranger to add from the Guide.
  # (The administration tool has no command for either, so the fixture writes the service's own tables.)
  python3 -I - "$db" <<'PY'
import sqlite3, sys, time, uuid
db = sqlite3.connect(sys.argv[1])
ids = dict(db.execute("select username, id from users"))
db.execute("insert into friends(user_id, friend_id) values (?, ?)", (ids["visitor"], ids["curator"]))
db.execute("insert into friends(user_id, friend_id) values (?, ?)", (ids["curator"], ids["visitor"]))
db.execute("insert into messages(id, sender_id, recipient_id, text, created) values (?, ?, ?, ?, ?)",
           (uuid.uuid4().hex, ids["curator"], ids["visitor"], "Welcome to the CNA 3D Museum. The Archivist keeps the rooms on floor 8.", int(time.time())))
db.commit()
PY
}

case ${1:-status} in
  start)
    [ -x "$bin/cna-gamer-services-server" ] || { echo "the service is not built: $bin/cna-gamer-services-server is missing (see the header of this script)" >&2; exit 3; }
    mkdir -p "$dir"
    if running; then echo "already running (pid $(cat "$pidfile"))"; exit 0; fi
    if [ "${2:-}" = "--fresh" ] || [ ! -f "$db" ]; then provision || { echo "provisioning failed" >&2; exit 1; }; fi
    setsid nohup "$bin/cna-gamer-services-server" --database "$db" --port "$port" --insecure-loopback > "$dir/server.log" 2>&1 < /dev/null &
    echo $! > "$pidfile"
    for _ in $(seq 1 50); do
      if (exec 3<>/dev/tcp/127.0.0.1/"$port") 2>/dev/null; then echo "service running on 127.0.0.1:$port (pid $(cat "$pidfile"))"; exit 0; fi
      sleep 0.2
    done
    echo "the service did not open port $port; see $dir/server.log" >&2; exit 1 ;;
  stop)
    if running; then kill "$(cat "$pidfile")"; rm -f "$pidfile"; echo stopped; else echo "not running"; fi ;;
  env)
    # The sign-in is remembered (CNA keeps a refresh credential), so the credentials and profiles stay in the build tree, not in
    # the desktop keyring or the user's own CNA folders.
    echo "export CNA_GAMER_SERVICES_ENDPOINT=http://127.0.0.1:$port/cna/v1 CNA_GAME_ID=$title CNA_GAMER_SERVICES_INSECURE_LOOPBACK=1" \
         "CNA_GAMER_SERVICES_CREDENTIALS_DIR=$dir/credentials CNA_GAMER_SERVICES_PROFILES_DIR=$dir/profiles" ;;
  status)
    if running; then echo "running (pid $(cat "$pidfile")) on 127.0.0.1:$port"; else echo "not running"; fi ;;
  *) echo "usage: $0 start [--fresh] | stop | status | env" >&2; exit 2 ;;
esac
