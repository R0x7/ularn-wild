package com.ularn.android.game

import com.ularn.android.cards.MonsterFacts
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class PetStatusTest {
    private val jackal = (1..MonsterFacts.COUNT).first { MonsterFacts.get(it)?.name == "jackal" }

    @Test fun titleNamesStageAndSpecies() {
        assertEquals("Baby jackal", PetStatus(jackal, 13, 13, 0, 0, 5).title)
        assertEquals("Young jackal", PetStatus(jackal, 13, 21, 1, 5, 15).title)
        assertEquals("Fearsome jackal", PetStatus(jackal, 30, 37, 3, 30, 0).title)
    }

    @Test fun titleSurvivesValuesTheAppDoesNotKnow() {
        assertEquals("Fearsome pet", PetStatus(0, 1, 1, 9, 0, 0).title)
        assertEquals("Baby pet", PetStatus(100000, 1, 1, -1, 0, 0).title)
    }

    @Test fun growthCountsKillsAndNamesTheNextStep() {
        assertEquals("0 kills · grows at 5", PetStatus(jackal, 13, 13, 0, 0, 5).growth)
        assertEquals("1 kill · grows at 5", PetStatus(jackal, 13, 13, 0, 1, 5).growth)
        assertEquals("7 kills · grows at 15", PetStatus(jackal, 13, 21, 1, 7, 15).growth)
        assertEquals("31 kills · fully grown", PetStatus(jackal, 30, 37, 3, 31, 0).growth)
    }

    @Test fun whereaboutsOnlyWhenLeftBehind() {
        assertNull(PetStatus(jackal, 13, 13, 0, 0, 5).whereabouts)
        assertEquals("Waiting on level 3", PetStatus(jackal, 13, 13, 0, 0, 5, "3").whereabouts)
        assertEquals("Waiting on level V1", PetStatus(jackal, 13, 13, 0, 0, 5, "V1").whereabouts)
        assertEquals("Waiting in town", PetStatus(jackal, 13, 13, 0, 0, 5, "H").whereabouts)
    }
}
