#!/bin/sh
# Regression: the poster the side panel's BOUNTY section shows
# (shared_state.h: bounty_info, written by wild_bounty.c: bounty_publish()).
# Seed 3's first poster wants a jackal (monster 4): it must be published as
# soon as the crier puts it up, unclaimed, with the turn it comes down at.
# A jackal created next to the player and annihilated (object 55 = scroll,
# arg 21) claims it: published as claimed. On the next turn in town the
# bounty is paid and the new poster (a kobold, monster 5) is published.
# With the bounty board off, or all Wild features off, nothing is published.
# Exit code != 0 on failure.
set -e
cd "$(dirname "$0")"
sh build.sh >/dev/null
fail() { echo "$out"; echo "FAIL: $1"; exit 1; }
pos() { echo "$out" | grep '^POS:'; }

out=$(./play.sh --seed 3 --wizard '=m4\n' '=o55\n21\n' 'r' '.') || true
echo "$out" | grep -q "WANTED, one jackal" || fail "seed 3 no longer posts a jackal: pick another seed"
pos | grep -q "bounty=4:125g:claimed0:downat25[0-9][0-9] " ||
  fail "the jackal poster should be published unclaimed, coming down about 2,500 turns after it went up"
echo "$out" | grep -q "That jackal was WANTED" || fail "the annihilated jackal did not claim the bounty"
pos | grep -q "bounty=4:125g:claimed1:downat0 " || fail "a claimed bounty should be published as claimed"
echo "$out" | grep -q "counts out your bounty: 125 gold" || fail "the bounty was not paid in town"
pos | tail -1 | grep -q "bounty=5:125g:claimed0:downat25[0-9][0-9] " ||
  fail "after the payout the new poster (a kobold) should be published"

# bounty board off (every other feature on), then the vanilla game
out=$(./play.sh --seed 3 --wild 2047 '.' '.' '.') || true
pos | grep -q "bounty=" && fail "a poster is published with the bounty board off"
out=$(./play.sh --seed 3 --wild 0 '.' '.' '.') || true
pos | grep -q "bounty=" && fail "a poster is published with the Wild features off"
echo "bounty_panel.sh: OK"
