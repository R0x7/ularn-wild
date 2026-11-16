/* =============================================================================
 * FILENAME: wild_pets.h
 *
 * Ularn: Wild Edition feature "pets": the one entry point upstream code calls
 * besides the hooks in wild.h (see wild_pets.c).
 * =============================================================================
 */
#ifndef ULARN_WILD_PETS_H
#define ULARN_WILD_PETS_H

/* The player walks into their pet at (x,y) (wild_is_pet(x,y) is true): the
 * pet moves into the player's square so the player can take its place.
 * Returns 1 when swapped, 0 when the pet can't go there (the move is
 * blocked, the pet is not attacked). */
int wild_pet_swap(int x, int y);

/* One line describing the pet for the known-items page (show.c), or NULL
 * when there is none (or WILDF_PETS is off). */
const char *wild_pet_text(void);

#endif
