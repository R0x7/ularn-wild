#!/bin/sh
# Regression: when rain or a storm clears over the town in daylight, a rainbow
# puts a pot of gold on the map (wild_weather.c: rainbow()). Lets 20,000 turns
# of weather pass in town (wizard debug menu "(t)urns", 1000 at a time) and
# checks that each rainbow (event WILD_WEATHER(0,hour,2)) comes in daylight and
# leaves exactly one new gold pile on the map; then that none comes while the
# player is underground or with the Weather feature off. Perks (and LarnCoin's
# price ticks) are off: a lightning kill's level-up prompt would stop the clock,
# and 1000 turns of ticks would overflow the event ring. Exit code != 0 on
# failure.
set -e
cd "$(dirname "$0")"
sh build.sh >/dev/null
ticks=""; i=0
while [ $i -lt 20 ]; do ticks="$ticks =t1000\\n"; i=$((i + 1)); done
fail() { echo "$out"; echo "FAIL: $1"; exit 1; }
piles() { echo "$1" | grep '^MAP:' | tr -cd '$' | wc -c; }   # $ = gold on the !map dump
rainbows() { echo "$1" | grep -c "EVENT: WILD_WEATHER(0,[0-9]*,2)" || true; }
MASK=3823   # 4095 without WILDF_PERKS (1<<8) and WILDF_COIN (1<<4)

# in town
before=$(./play.sh --seed 5 --wild $MASK --wizard --quiet '!map') || true
out=$(./play.sh --seed 5 --wild $MASK --wizard --quiet $ticks '!map') || true
n=$(rainbows "$out")
[ "$n" -ge 1 ] || fail "no rainbow in 20000 turns in town"
echo "$out" | grep -q "a rainbow arches over the town!" || fail "a rainbow without its message"
echo "$out" | grep -q "Its end touches the ground to the [a-z-]*\. Is that a pot of gold?" ||
  fail "a rainbow without a direction"
echo "$out" | grep -q "ground to the here" && fail "a pot of gold under the player"
night=$(echo "$out" | grep "EVENT: WILD_WEATHER(0,[0-9]*,2)" |
  sed 's/.*(0,\([0-9]*\),2).*/\1/' | awk '$1 >= 21 || $1 < 6' | wc -l)
[ "$night" = 0 ] || fail "$night rainbows at night"
b=$(piles "$before"); a=$(piles "$out")
[ $((a - b)) = "$n" ] || fail "$n rainbows but gold piles on the map went from $b to $a"

# underground (object 79 = the dungeon entrance): the weather changes, unseen
out=$(./play.sh --seed 5 --wild $MASK --wizard --quiet '=o79\n0\ng' $ticks) || true
echo "$out" | grep -q "EVENT: NEW_LEVEL(1" || fail "never reached level 1"
[ "$(rainbows "$out")" = 0 ] || fail "a rainbow while underground"

# Weather feature (1<<9) off
out=$(./play.sh --seed 5 --wild $((MASK & ~512)) --wizard --quiet $ticks) || true
echo "$out" | grep -q "rainbow" && fail "a rainbow with the Weather feature off"
echo "rainbow.sh: OK ($n rainbows in town, gold piles $b -> $a)"
