#!/bin/sh
# Regression: storm lightning is not the player's blow (wild_weather.c:
# lightning() -> wild.c: wild_hit_monster()). It went through hitm() as if
# it were, and hitm() ends the player's hold monster spell when it hurts a
# monster (c[HOLDMONST] = 1, cancelled by the next regen) and halves the
# damage under the half-damage curse.
# A scroll of hold monster is read in town, three tough monsters are made
# next to the player, and 20,000 turns of weather pass (wizard "(t)urns",
# which runs wild_turn() only, so neither regen() nor the monsters wear the
# spell down). There must be lightning strikes, and the hold monster spell
# must still be at what it was before them (the wizard "(f)lags" dump).
# Perks and LarnCoin are off, as in rainbow.sh. Exit code != 0 on failure.
set -e
cd "$(dirname "$0")"
sh build.sh >/dev/null
ticks=""; i=0
while [ $i -lt 20 ]; do ticks="$ticks =t1000\\n"; i=$((i + 1)); done
fail() { echo "$out"; echo "FAIL: $1"; exit 1; }
MASK=3823   # 4095 without WILDF_PERKS (1<<8) and WILDF_COIN (1<<4)
# the (f)lags line wraps over two MSG lines: join them, then read "hold N"
hold() { echo "$1" | grep '^MSG: ' | sed 's/^MSG: //' | tr -d '\n' |
  sed 's/.*hold \([0-9-]*\) halfdam.*/\1/'; }

# object 55 = scroll, arg 16 = hold monster; monsters 23 = troll, 45 = gnome king
setup="=o55\\n16\\n r =m23\\n =m23\\n =m45\\n"
out=$(./play.sh --seed 5 --wild $MASK --wizard --quiet $setup '=f') || true
before=$(hold "$out")
[ "${before:-0}" -gt 1 ] || fail "the scroll of hold monster did not take (hold=$before)"

out=$(./play.sh --seed 5 --wild $MASK --wizard --quiet $setup $ticks '=f') || true
strikes=$(echo "$out" | grep -c "A bolt of lightning strikes the" || true)
[ "$strikes" -ge 1 ] || fail "no lightning struck a monster in 20000 turns: pick another seed"
after=$(hold "$out")
[ "$after" = "$before" ] ||
  fail "$strikes lightning strike(s) changed the hold monster spell from $before to $after"
echo "lightning_hold.sh: OK ($strikes strikes, hold monster stayed at $before)"
