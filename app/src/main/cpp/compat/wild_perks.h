/* =============================================================================
 * FILENAME: wild_perks.h
 *
 * Ularn: Wild Edition feature "level-up perks" (docs/WILD_EDITION_PLAN.md,
 * 3.9). Small helpers the upstream code calls at the shared insertion
 * points. Every one of them is a no-op (or returns the neutral value) when
 * the WILDF_PERKS bit is off, so the calls need no guard of their own.
 * =============================================================================
 */
#ifndef ULARN_WILD_PERKS_H
#define ULARN_WILD_PERKS_H

/* Perk indices: bit (1 << PERK_X) of c[WILD_PERKS]. Mirrored in Kotlin
 * (SidePanelView.PERK_NAMES); the order is part of the save. */
#define PERK_MAGNET   0
#define PERK_SKIN     1
#define PERK_VAMPIRE  2
#define PERK_BOOKWORM 3
#define PERK_LUCKY    4
#define PERK_NIMBLE   5
#define PERK_HAGGLER  6
#define PERK_CANNON   7
#define PERK_WIND     8
#define PERK_OWL      9
#define PERK_MAPPER  10
#define PERK_COUNT    11

/* Non-zero when the character has perk `p` (and the feature is on). */
int  wild_perk(int p);

/* Name of perk `p` when the character has it, else NULL (show.c). */
const char *wild_perk_owned_name(int p);

/* recalc(): Thick Skin's armour class and Glass Cannon's weapon class. */
int  wild_perks_ac(void);
int  wild_perks_wc(void);

/* cast(): 1 when Bookworm makes this spell free (prints a message). */
int  wild_perks_free_spell(void);

/* hitm(): 1 when Lucky rolls the monster's drop a second time. */
int  wild_perks_lucky(void);

/* DND store: the price after Haggler's 10% discount. */
long wild_perks_price(long price);

/* Trading post: what an item fetches with Haggler (10% more). */
long wild_perks_sell(long value);

#endif
