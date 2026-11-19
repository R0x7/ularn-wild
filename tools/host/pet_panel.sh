#!/bin/sh
# Regression: the pet's status for the side panel's PET section
# (shared_state.h: pet_info / pet_level, written by wild_pets.c:
# pet_publish()). No pet: nothing published. A hatched pet: species, hit
# points within its maximum, a baby with no kills that grows at 5, and no
# level name while it is with the player. Left behind in town (it is kept
# busy by floating eyes, monster 12, while the player walks off and takes
# the dungeon entrance, object 79): the level it waits on is "H"; back in
# town (one step down from where level 1 is entered is the way out) that is
# cleared again. Exit code != 0 on failure.
set -e
cd "$(dirname "$0")"
sh build.sh >/dev/null
fail() { echo "$out"; echo "FAIL: $1"; exit 1; }

# a tough character (12 levels: the eyes bite), an egg, 300 turns to hatch it.
# The seed picks which pet hatches; a weak one can die to the eyes before the
# player is back (seed 9 did once a twelfth level-up became an eleventh perk)
start="{12:=x} . {12:a}"
out=$(./play.sh --seed 1 --wizard --class f $start '.' \
  '=o111\n0\nt' '=t300\n' '.' \
  '=m12\n' '=m12\n' '=m12\n' 'h' 'h' 'h' '=o79\n0\ng' '.' \
  'j' '.' '.') || true
pos() { echo "$out" | grep '^POS:'; }
pos | head -3 | grep -q "petinfo=" && fail "a pet is published before the egg hatched"
echo "$out" | grep -q "EVENT: WILD_PET(0," || fail "the egg did not hatch"
pos | grep -q "level= H .* petinfo=[0-9]*:\([0-9]*\)/\1:stage0:kills0:next5:at\[\] " ||
  fail "a new pet should be a baby at full hit points, with no kills, growing at 5, on no other level"
echo "$out" | grep -q "sad whine" || fail "the pet was not left behind: adjust the steps"
pos | grep -q "level= 1 .* pet=(-1,-1) .* petinfo=[0-9:/a-z]*:at\[H\] " ||
  fail "a pet left in town should be published as waiting on level H"
pos | grep "level= 1 " | grep -q "at\[\]" && fail "the level name was dropped while the pet is still in town"
echo "$out" | grep -q "EVENT: NEW_LEVEL(0" || fail "never came back to town: adjust the steps"
pos | tail -1 | grep -q "level= H .* petinfo=[0-9:/a-z]*:at\[\] " ||
  fail "back on the pet's level, it should not be published as waiting elsewhere"
# every published pet has hit points within its maximum
pos | sed -n 's/.* petinfo=[0-9]*:\([0-9]*\)\/\([0-9]*\):.*/\1 \2/p' | while read hp max; do
  [ "$hp" -ge 1 ] && [ "$hp" -le "$max" ] || { echo "FAIL: pet hit points $hp of $max"; exit 1; }
done

# feature off: nothing published
out=$(./play.sh --seed 9 --wild 0 '.' '.') || true
pos | grep -q "petinfo=" && fail "a pet is published with the Wild features off"
echo "pet_panel.sh: OK"
