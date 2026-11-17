package com.ularn.android.game

import com.ularn.android.cards.MonsterFacts

/**
 * The player's pet (Wild Edition, compat/wild_pets.c) as the side panel's PET
 * section shows it. No Android classes: unit-tested on the JVM.
 */
data class PetStatus(
    /** Monster id of the pet's species. */
    val species: Int,
    /** Hit points; as last seen while the pet is on another level. */
    val hp: Int,
    val hpMax: Int,
    /** 0 baby, 1 young, 2 grown, 3 fearsome (stage_name[] in wild_pets.c). */
    val stage: Int,
    /** Monsters the pet has killed with its own bite. */
    val kills: Int,
    /** The kill count it grows at next; 0 = fully grown. */
    val growsAt: Int,
    /** Name of the level it was left on ("H", "3", "V1"); "" = with the player. */
    val leftOn: String = "",
) {
    /** "Young jackal". */
    val title: String get() {
        val name = MonsterFacts.get(species)?.name ?: "pet"
        return "${STAGES[stage.coerceIn(0, STAGES.lastIndex)]} $name"
    }

    /** "7 kills · grows at 15", "1 kill · grows at 5", "31 kills · fully grown". */
    val growth: String get() {
        val k = "$kills ${if (kills == 1) "kill" else "kills"}"
        return if (growsAt > 0) "$k · grows at $growsAt" else "$k · fully grown"
    }

    /** Where the pet waits when it isn't on the player's level, else null. */
    val whereabouts: String? get() = when (leftOn) {
        "" -> null
        "H" -> "Waiting in town"
        else -> "Waiting on level $leftOn"
    }

    private companion object {
        val STAGES = listOf("Baby", "Young", "Grown", "Fearsome")
    }
}
