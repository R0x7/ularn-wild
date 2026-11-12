#!/bin/sh
# Regression: the Second Wind perk saves the character once per dungeon level
# for the whole game. It used to remember only the level of its latest use,
# so using it on another level armed it again on the first one.
# Takes every perk (ten level-ups), then takes a deadly blow (wizard debug
# menu, "(d)amage") on level 1, on level 2 and back on level 1: the first two
# must be survived, the third must kill. Exit code != 0 on failure.
set -e
cd "$(dirname "$0")"
sh build.sh >/dev/null
out=$(./play.sh --seed 5 --wizard --class f --quiet \
  '{10:=x}' '.' '{10:a}' \
  '=o79\n0\ng' '=d9999\n' '=h' \
  '=o13\n0\nd' '=d9999\n' '=h' \
  '=o7\n0\nu' '=d9999\n') || true
fail() { echo "$out"; echo "FAIL: $1"; exit 1; }
# WILD_PERK(8,1,..) is the event Second Wind sends each time it saves you
n=$(echo "$out" | grep -c "EVENT: WILD_PERK(8,1," || true)
echo "$out" | grep -q "EVENT: NEW_LEVEL(2" || fail "never reached level 2"
[ "$(echo "$out" | grep -c "EVENT: NEW_LEVEL(1")" = 2 ] || fail "never came back to level 1"
[ "$n" = 2 ] || fail "Second Wind fired $n times, expected 2 (levels 1 and 2)"
echo "$out" | grep -q "EVENT: DIED" || fail "the second deadly blow on level 1 did not kill"
echo "second_wind.sh: OK"
