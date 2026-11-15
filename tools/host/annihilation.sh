#!/bin/sh
# Regression: a monster destroyed by a scroll of annihilation is a kill for
# the Wild features. annihilate() removes its victims without hitm(), so it
# used to give the experience and nothing else: no KILL event, no card, no
# bounty, and a roomba took everything it had eaten with it.
#  1. Seed 3's first poster wants a jackal. One is created next to the
#     player and annihilated: the kill must be reported, drop the species'
#     first card and claim the bounty.
#  2. A roomba eats a can of Fizz and is annihilated: it must cough the can
#     back up next to where it stood (seen as a new tile around the player).
# Exit code != 0 on failure.
set -e
cd "$(dirname "$0")"
sh build.sh >/dev/null
fail() { echo "$out"; echo "FAIL: $1"; exit 1; }

# object 55 = scroll, arg 21 = annihilation; monster 4 = jackal
out=$(./play.sh --seed 3 --wizard --quiet '=m4\n' '=o55\n21\n' 'r' '.')
echo "$out" | grep -q "WANTED, one jackal" || fail "seed 3 no longer posts a jackal: pick another seed"
echo "$out" | grep -q "You read a scroll of annihilation" || fail "the scroll was not read"
echo "$out" | grep -q "EVENT: KILL(4," || fail "the annihilated jackal was not reported as a kill"
echo "$out" | grep -q "EVENT: WILD_CARD(4," || fail "the first jackal killed dropped no card"
echo "$out" | grep -q "That jackal was WANTED" || fail "the annihilated jackal did not claim the bounty"
echo "$out" | grep -q "counts out your bounty: 125 gold" || fail "the bounty was not paid in town"

# object 102 = can of Fizz, monster 60 = roomba (it comes over and eats the can)
out=$(./play.sh --seed 3 --wizard --quiet '=o102\n0\n' 'i' '=m60\n' 'i' 'u' '.' '.' '.' \
  '!tiles' '=o55\n21\n' 'r' '!tiles')
echo "$out" | grep -q "EVENT: WILD_VENDING(3,102," || fail "the roomba never ate the can: adjust the steps"
echo "$out" | grep -q "EVENT: KILL(60," || fail "the annihilated roomba was not reported as a kill"
echo "$out" | grep -q "coughs up its contents" || fail "the roomba did not cough up what it ate"
# the tiles around the player, after the roomba ate (nothing lying about)
# and after it was annihilated (the can is back)
before=$(echo "$out" | grep "^TILES:" | sed -n '1,5p')
after=$(echo "$out" | grep "^TILES:" | sed -n '6,10p')
[ -n "$after" ] || fail "no tile dump after the scroll"
[ "$before" != "$after" ] || fail "nothing changed on the floor"
n=$(echo "$after" | tr ' ' '\n' | grep -c '^[0-9][0-9]*$')
# every cell is grass (191) except the player's and the dropped can's
grass=$(echo "$after" | tr ' ' '\n' | grep -c '^191$')
[ $((n - grass)) = 2 ] || fail "expected the player and one can on the grass, found $((n - grass)) non-grass tiles"
echo "annihilation.sh: OK"
