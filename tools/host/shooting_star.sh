#!/bin/sh
# Regression: on a clear night in town a shooting star now and then falls and
# leaves a gem on the map (wild_weather.c: shooting_star()). Lets 20,000 turns
# of weather pass in town (wizard debug menu "(t)urns", 1000 at a time) and
# checks that each star (event WILD_WEATHER(0,hour,3)) falls at night under a
# clear sky, with its message and a direction, and leaves exactly one new item
# on the map; then that none falls while the player is underground or with
# the Weather feature off. Perks and LarnCoin are off, as in rainbow.sh.
# Exit code != 0 on failure.
set -e
cd "$(dirname "$0")"
sh build.sh >/dev/null
ticks=""; i=0
while [ $i -lt 20 ]; do ticks="$ticks =t1000\\n"; i=$((i + 1)); done
fail() { echo "$out"; echo "FAIL: $1"; exit 1; }
items() { echo "$1" | grep '^MAP:' | tr -cd 'o' | wc -c; }   # o = an item on the !items dump (monsters don't hide it)
stars() { echo "$1" | grep -c "EVENT: WILD_WEATHER(0,[0-9]*,3)" || true; }
MASK=3823   # 4095 without WILDF_PERKS (1<<8) and WILDF_COIN (1<<4)

# in town
before=$(./play.sh --seed 5 --wild $MASK --wizard --quiet '!items') || true
out=$(./play.sh --seed 5 --wild $MASK --wizard --quiet $ticks '!items') || true
n=$(stars "$out")
[ "$n" -ge 1 ] || fail "no shooting star in 20000 turns in town"
echo "$out" | grep -q "A shooting star streaks across the sky!" || fail "a star without its message"
echo "$out" | grep -q "It falls to earth to the [a-z-]*\. Something glitters there\." ||
  fail "a star without a direction"
echo "$out" | grep -q "earth to the here" && fail "a star under the player"
day=$(echo "$out" | grep "EVENT: WILD_WEATHER(0,[0-9]*,3)" |
  sed 's/.*(0,\([0-9]*\),3).*/\1/' | awk '$1 < 21 && $1 >= 6' | wc -l)
[ "$day" = 0 ] || fail "$day shooting stars in daylight"
# the weather in force when each star fell: the last change before it
cloudy=$(echo "$out" | grep "EVENT: WILD_WEATHER(" | awk -F'[(,)]' '
  $4 == 0 { w = $2 } $4 == 3 && w != 0 { n++ } END { print n + 0 }')
[ "$cloudy" = 0 ] || fail "$cloudy shooting stars through clouds"
b=$(items "$before"); a=$(items "$out")
[ $((a - b)) = "$n" ] || fail "$n shooting stars but items on the map went from $b to $a"

# underground (object 79 = the dungeon entrance): no sky to see
out=$(./play.sh --seed 5 --wild $MASK --wizard --quiet '=o79\n0\ng' $ticks) || true
echo "$out" | grep -q "EVENT: NEW_LEVEL(1" || fail "never reached level 1"
[ "$(stars "$out")" = 0 ] || fail "a shooting star while underground"

# Weather feature (1<<9) off
out=$(./play.sh --seed 5 --wild $((MASK & ~512)) --wizard --quiet $ticks) || true
echo "$out" | grep -q "shooting star" && fail "a shooting star with the Weather feature off"
echo "shooting_star.sh: OK ($n stars in town, items on the map $b -> $a)"
