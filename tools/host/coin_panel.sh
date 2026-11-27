#!/bin/sh
# Regression: the LarnCoin the side panel's LARNCOIN section shows
# (shared_state.h: coin_held, written by wild_coin.c: wild_coin_paint()).
# A trade at the ATM (object 117, placed under the player) is not followed
# by a map repaint, so it must publish the new holdings itself: 5 coins
# right after buying them (with the HUD ticker shown), none right after
# selling them. With LarnCoin off nothing is published.
# Exit code != 0 on failure.
set -e
cd "$(dirname "$0")"
sh build.sh >/dev/null
fail() { echo "$out"; echo "FAIL: $1"; exit 1; }

out=$(./play.sh --seed 3 --wizard '=g' '=o117\n0\n' 'b' '5\n' 'h' 'l' 's' '5\n') || true
echo "$out" | grep -q "You buy 5 LarnCoin" || fail "the ATM did not sell 5 LarnCoin"
pos=$(echo "$out" | grep '^POS:')
echo "$pos" | sed -n '6p' | grep -q "coin=100 .*coinheld=5 " ||
  fail "5 coins (and the ticker) should be published right after buying them"
echo "$pos" | tail -1 | grep -q "coinheld=" && fail "no coins should be published after selling them all"
echo "$pos" | tail -1 | grep -q "coin=-100 " || fail "the ticker should hide after selling every coin"

# LarnCoin off (every other feature on)
out=$(./play.sh --seed 3 --wild 4079 --wizard '=g' '=o117\n0\n' 'b' '5\n') || true
echo "$out" | grep '^POS:' | grep -q "coinheld=" && fail "coins are published with LarnCoin off"
echo "coin_panel.sh: OK"
