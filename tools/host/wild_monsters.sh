#!/bin/sh
# Regression: the Wild Edition's monsters (null pointer, fountain kraken, tax
# auditor, roomba) are never made at random. They sit between the red dragon
# and the demon lords in the monster table, so createmonster() and
# makemonst(), which step to the next species when the one they picked has
# been genocided, used to walk from a genocided red dragon into a null
# pointer, even with every Wild feature off. The base game gets a type I
# demon lord there, and so must this one.
#  1. Wild features off: red dragons (monster 56) are genocided, then one is
#     asked for. What turns up and attacks must be the demon lord.
#  2. The same with the features on.
# Exit code != 0 on failure.
set -e
cd "$(dirname "$0")"
sh build.sh >/dev/null
fail() { echo "$out"; echo "FAIL: $1"; exit 1; }

for mask in 0 4095; do
  # '_' makes the wizard The Creator (every spell known); class b casts
  # genocide reliably; perks are left out of the mask so no prompt interrupts
  seed=3
  [ $mask = 0 ] || { mask=$((4095 - 256)); seed=1; }
  out=$(./play.sh --seed $seed --wild $mask --class b --wizard --quiet '_' 'cgen' '56\n' '=m56\n' ',' ',') || true
  echo "$out" | grep -q "There will be no more red dragons" || fail "wild=$mask: the genocide spell failed with seed $seed: pick another"
  echo "$out" | grep -q "Created red dragon" || fail "wild=$mask: the debug menu did not create the monster"
  echo "$out" | grep -qi "null pointer\|fountain kraken\|tax auditor\|roomba" &&
    fail "wild=$mask: a Wild monster took the genocided red dragon's place"
  echo "$out" | grep -q "The red dragon " && fail "wild=$mask: a genocided red dragon turned up"
  echo "$out" | grep -q "type I demon lord" || fail "wild=$mask: expected a type I demon lord, as in the base game"
done
echo "wild_monsters.sh: OK"
