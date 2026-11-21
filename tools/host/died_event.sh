#!/bin/sh
# Regression: the app hears DIED (game-over music, the auto photo of the
# final moment) only when the game is really over. died() used to send it
# first thing, so a life preserver bringing the character back, a save and
# a quit all played the game-over music and took a "final moment" photo.
#  1. A scroll of life protection, then a deadly blow: revived, no DIED.
#     A second deadly blow kills: DIED.
#  2. A save (S): no DIED.
#  3. A quit (Q, y): no DIED.
# Exit code != 0 on failure.
set -e
cd "$(dirname "$0")"
sh build.sh >/dev/null
fail() { echo "$out"; echo "FAIL: $1"; exit 1; }

# object 55 = scroll, arg 23 = life protection
out=$(./play.sh --seed 3 --wizard --quiet '=o55\n23\n' 'r' '=d9999\n' '.') || true
echo "$out" | grep -q "You read a scroll of life protection" || fail "the scroll was not read"
echo "$out" | grep -q "wiiieeeeerrrrrd" || fail "the life preserver did not bring the character back"
! echo "$out" | grep -q "EVENT: DIED" || fail "a character the life preserver saved was reported dead"
out=$(./play.sh --seed 3 --wizard --quiet '=o55\n23\n' 'r' '=d9999\n' '=d9999\n') || true
echo "$out" | grep -q "EVENT: DIED" || fail "the second deadly blow was not reported"

H=$(mktemp -d /tmp/ularn_died_XXXX)
out=$(./play.sh --seed 3 --quiet --home "$H" '.' 'S') || true
[ -f "$H/ularn_player.sav" ] || fail "no save file"
! echo "$out" | grep -q "EVENT: DIED" || fail "saving the game was reported as a death"
rm -rf "$H"

out=$(./play.sh --seed 3 --quiet '.' 'Q' 'y') || true
echo "$out" | grep -q "really want to quit" || fail "the quit prompt never came up"
! echo "$out" | grep -q "EVENT: DIED" || fail "quitting was reported as a death"
echo "died_event.sh: OK"
