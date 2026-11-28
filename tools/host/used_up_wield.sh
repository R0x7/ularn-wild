#!/bin/sh
# Regression: a wielded Wild item that gets used up (a slice of pizza eaten,
# an egg that hatches) is unwielded too (wild.c: wild_use_up()). It used to
# leave c[WIELD] pointing at the empty slot, so take() put the next item
# picked up there "(weapon in hand)" without the player wielding it.
# Each case wields the item, uses it up, then picks up a dagger: the dagger
# must not be in hand. Exit code != 0 on failure.
set -e
cd "$(dirname "$0")"
sh build.sh >/dev/null
fail() { echo "$out"; echo "FAIL: $1"; exit 1; }
# objects 103 = slice of pizza, 111 = mysterious egg, 39 = dagger; slot c is
# the first free one (the character starts with two potions)
dagger='=o39\n0\n t !inv'

out=$(./play.sh --seed 3 --wizard --quiet '=o103\n0\n' t w c e c $dagger) || true
echo "$out" | grep -q "You eat the slice of pizza" || fail "the pizza was not eaten"
echo "$out" | grep '^INV: c) a dagger' | grep -q "in hand" && fail "eating a wielded pizza left the dagger picked up after it in hand"

out=$(./play.sh --seed 3 --wizard --quiet '=o111\n0\n' t w c '=t400\n' $dagger) || true
echo "$out" | grep -q "The egg hatches" || fail "the egg did not hatch"
echo "$out" | grep '^INV: c) a dagger' | grep -q "in hand" && fail "a wielded egg hatching left the dagger picked up after it in hand"
echo "used_up_wield.sh: OK"
