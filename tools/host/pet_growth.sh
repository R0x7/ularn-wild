#!/bin/sh
# Regression: a pet grows up by fighting (wild_pets.c: pet_made_kill()). An
# egg is hatched (object 111 = OEGG, carried for 300 wild_turn() ticks), then
# kobolds (monster 5) are created next to the player, who only rests: every
# kill is the pet's own bite. After the 5th the pet must grow exactly once,
# and the known-items page ('\') must say so before and after a save and
# restore. Exit code != 0 on failure.
set -e
cd "$(dirname "$0")"
sh build.sh >/dev/null
HOME_DIR=$(mktemp -d /tmp/ularn_pet_XXXX)
fail() { echo "$out"; echo "FAIL: $1"; rm -rf "$HOME_DIR"; exit 1; }
fight='=m5\n {8:.}'   # two steps: a kobold appears, the player rests 8 turns

# four kills: still a baby
out=$(./play.sh --seed 9 --wizard --class f --quiet '=o111\n0\nt' '=t300\n' '.' \
  $fight $fight $fight $fight '\\' '\s') || true
echo "$out" | grep -q "EVENT: WILD_PET(0," || fail "the egg did not hatch"
[ "$(echo "$out" | grep -c "EVENT: KILL(5,")" = 4 ] || fail "expected 4 kobolds killed by the pet: adjust the steps"
echo "$out" | grep -q "has grown" && fail "the pet grew before its 5th kill"
echo "$out" | grep -q "TEXT: A baby [a-z ]*: [0-9]* of [0-9]* hit points, 4 kills (it grows at 5)\." ||
  fail "the known-items page does not show a baby with 4 kills"
max0=$(echo "$out" | sed -n 's/^TEXT: A baby [a-z ]*: [0-9]* of \([0-9]*\) hit points.*/\1/p' | tail -1)

# the fifth kill: it grows, once; three more kills change nothing; then save
out=$(./play.sh --seed 9 --wizard --class f --quiet --home "$HOME_DIR" '=o111\n0\nt' '=t300\n' '.' \
  $fight $fight $fight $fight $fight $fight $fight $fight '\\' '\s' 'S') || true
[ "$(echo "$out" | grep -c "EVENT: KILL(5,")" = 8 ] || fail "expected 8 kobolds killed by the pet: adjust the steps"
# WILD_PET(4,species,stage) is the event each growth sends (--quiet repeats
# a message while it stays on screen, so the messages can't be counted)
[ "$(echo "$out" | grep -c "EVENT: WILD_PET(4,")" = 1 ] || fail "the pet did not grow exactly once in 8 kills"
echo "$out" | grep -q "EVENT: WILD_PET(4,[0-9]*,1)" || fail "the grow event does not say stage 1"
echo "$out" | grep -q "has grown! It is a young " || fail "growth without its message"
echo "$out" | grep -q "TEXT: A young [a-z ]*: [0-9]* of [0-9]* hit points, 8 kills (it grows at 15)\." ||
  fail "the known-items page does not show a young pet with 8 kills"
max1=$(echo "$out" | sed -n 's/^TEXT: A young [a-z ]*: [0-9]* of \([0-9]*\) hit points.*/\1/p' | tail -1)
[ "$max1" = $((max0 + 8)) ] || fail "a young pet should have 8 more hit points than a baby ($max0 -> $max1)"
[ -f "$HOME_DIR/ularn_player.sav" ] || fail "no save file"

# restored: still young, kills kept, and the pet is still recognised as ours
out=$(./play.sh --home "$HOME_DIR" --quiet '.' '.' '\\' '\s') || true
echo "$out" | grep -q "is gone" && fail "the pet was lost over a save and restore"
echo "$out" | grep -q "TEXT: A young [a-z ]*: [0-9]* of $max1 hit points, 8 kills (it grows at 15)\." ||
  fail "the pet's growth did not survive a save and restore"

# feature off: no pet line, no crash
out=$(./play.sh --seed 9 --wild 0 --quiet '\\' '\s') || true
echo "$out" | grep -q "Your pet" && fail "a pet line with the Wild features off"
rm -rf "$HOME_DIR"
echo "pet_growth.sh: OK (max hit points $max0 -> $max1)"
