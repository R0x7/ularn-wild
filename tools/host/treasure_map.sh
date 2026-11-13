#!/bin/sh
# Regression: a message in a bottle read below ground is sometimes a treasure
# map (wild_fishing.c: treasure_map()). Each map puts one pile of gold on the
# level and marks it on the player's map; above ground a bottle is always a
# fortune. Reads 30 bottles in town and 30 on dungeon level 1 (wizard debug
# menu, "(o)bject" 115 = OBOTTLE) and counts. Exit code != 0 on failure.
set -e
cd "$(dirname "$0")"
sh build.sh >/dev/null
bottles=""; i=0
while [ $i -lt 30 ]; do bottles="$bottles =o115\\n0\\nr"; i=$((i + 1)); done   # one step each
fail() { echo "$out"; echo "FAIL: $1"; exit 1; }
piles() { echo "$1" | grep '^MAP:' | tr -cd '$' | wc -c; }   # $ = gold on the !map dump

# in town: never a map
out=$(./play.sh --seed 11 --wizard --class f --quiet $bottles) || true
echo "$out" | grep -q "slips from your fingers" || fail "no bottle was read in town"
echo "$out" | grep -q "map of this very level" && fail "a bottle read in town was a treasure map"

# dungeon level 1 (object 79 = the dungeon entrance): gold piles before and after
before=$(./play.sh --seed 11 --wizard --class f --quiet '=o79\n0\ng' '!map') || true
out=$(./play.sh --seed 11 --wizard --class f --quiet '=o79\n0\ng' $bottles '!map') || true
echo "$out" | grep -q "EVENT: NEW_LEVEL(1" || fail "never reached level 1"
# WILD_FISHING(3,x,y) is the event each treasure map sends (--quiet prints a
# message only when it changed, so the messages can't be counted)
maps=$(echo "$out" | grep -c "EVENT: WILD_FISHING(3," || true)
[ "$maps" -ge 2 ] && [ "$maps" -le 18 ] || fail "$maps of 30 bottles were maps, expected about 7"
echo "$out" | grep -q "map of this very level" || fail "a map without its message"
echo "$out" | grep -q "An X marks a spot to the [a-z-]*\. You mark it" || fail "a map without a direction"
echo "$out" | grep -q "spot to the here" && fail "a stash under the player"
b=$(piles "$before"); a=$(piles "$out")
[ $((a - b)) = "$maps" ] || fail "$maps maps but gold piles on the map went from $b to $a"
echo "treasure_map.sh: OK ($maps maps, gold piles $b -> $a)"
