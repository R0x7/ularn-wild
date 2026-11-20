package com.ularn.android.game

import com.ularn.android.cards.MonsterFacts
import java.util.Locale

/**
 * The town crier's WANTED poster (Wild Edition, compat/wild_bounty.c) as the
 * side panel's BOUNTY section shows it. No Android classes: unit-tested on
 * the JVM.
 */
data class BountyStatus(
    /** Monster id of the wanted species. */
    val species: Int,
    /** Reward in gold. */
    val gold: Int,
    /** One has been killed: the gold waits in town. */
    val claimed: Boolean,
    /** The turn after which an unanswered poster comes down at the next town
     *  visit; 0 = claimed, or it comes down at the next visit anyway. */
    val downAt: Int,
) {
    /** "WANTED: kobold". */
    val title: String get() = "WANTED: ${MonsterFacts.get(species)?.name ?: "monster"}"

    /** "Reward 125 gold". */
    val reward: String get() = "Reward ${grouped(gold)} gold"

    /** What happens next, at turn [now]: collect it, or when the poster comes down. */
    fun outlook(now: Int): String = when {
        claimed -> "Done! Collect it in town"
        downAt <= now -> "Comes down on your next town visit"
        else -> {
            val left = downAt - now
            "Comes down in ${grouped(left)} ${if (left == 1) "turn" else "turns"}"
        }
    }

    /** 3,700 whatever the phone's language (the game's own text is English). */
    private fun grouped(n: Int) = String.format(Locale.ROOT, "%,d", n)
}
