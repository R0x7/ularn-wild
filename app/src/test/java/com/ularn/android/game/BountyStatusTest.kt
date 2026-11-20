package com.ularn.android.game

import com.ularn.android.cards.MonsterFacts
import org.junit.Assert.assertEquals
import org.junit.Test

class BountyStatusTest {
    private val kobold = (1..MonsterFacts.COUNT).first { MonsterFacts.get(it)?.name == "kobold" }

    @Test fun titleNamesTheWantedSpecies() {
        assertEquals("WANTED: kobold", BountyStatus(kobold, 125, false, 2501).title)
        assertEquals("WANTED: monster", BountyStatus(100000, 125, false, 2501).title)
    }

    @Test fun rewardGroupsThousands() {
        assertEquals("Reward 125 gold", BountyStatus(kobold, 125, false, 2501).reward)
        assertEquals("Reward 3,700 gold", BountyStatus(kobold, 3700, false, 2501).reward)
    }

    @Test fun outlookCountsDownToThePosterComingDown() {
        val b = BountyStatus(kobold, 125, false, 2501)
        assertEquals("Comes down in 2,500 turns", b.outlook(1))
        assertEquals("Comes down in 1 turn", b.outlook(2500))
        assertEquals("Comes down on your next town visit", b.outlook(2501))
        assertEquals("Comes down on your next town visit", b.outlook(9000))
    }

    @Test fun genocidedSpeciesComesDownAtTheNextVisit() {
        assertEquals("Comes down on your next town visit", BountyStatus(kobold, 125, false, 0).outlook(10))
    }

    @Test fun claimedBountyWaitsInTown() {
        assertEquals("Done! Collect it in town", BountyStatus(kobold, 125, true, 0).outlook(10))
    }
}
