#!/bin/sh
# Regression: the Cartographer perk marks a level's stairs on the map when
# you arrive. Takes every perk (eleven level-ups), stops being a wizard (a
# wizard sees every new level whole) and walks into the dungeon: level 1's
# stairs down must be on the map at once. The same walk with the perks
# feature off must show only what is next to you. Exit code != 0 on failure.
set -e
cd "$(dirname "$0")"
sh build.sh >/dev/null
walk="'=o79\n0\ni' '=w' 'l' 'h' 'g' '!map'"
with=$(eval ./play.sh --seed 5 --wizard --class f --quiet "'{11:=x}' '.' '{11:a}'" "$walk") || true
without=$(eval ./play.sh --seed 5 --wizard --class f --quiet --wild 3839 "'{11:=x}'" "$walk") || true
fail() { echo "$1"; echo "FAIL: $2"; exit 1; }
echo "$with" | grep -q "EVENT: NEW_LEVEL(1" || fail "$with" "never reached level 1"
echo "$with" | grep -q "EVENT: WILD_PERK(10,0," || fail "$with" "never took Cartographer"
echo "$with" | grep -q "EVENT: WILD_PERK(10,[1-9]" || fail "$with" "Cartographer marked nothing on level 1"
echo "$with" | grep "^MAP:" | grep -q ">" || fail "$with" "the stairs down are not on the map"
echo "$without" | grep -q "EVENT: NEW_LEVEL(1" || fail "$without" "never reached level 1 (perks off)"
echo "$without" | grep "^MAP:" | grep -q ">" && fail "$without" "stairs on the map with the perks feature off"
echo "cartographer.sh: OK"
