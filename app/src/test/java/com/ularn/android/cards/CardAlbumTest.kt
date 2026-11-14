package com.ularn.android.cards

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class CardAlbumTest {

    @Test fun firstDropIsNewThenCounts() {
        val a = CardAlbum()
        val d1 = a.add(1, holo = false, runId = 42, now = 1000L)!!
        assertTrue(d1.newCard); assertFalse(d1.newHolo); assertEquals(1, d1.count)
        val d2 = a.add(1, holo = true, runId = 43, now = 2000L)!!
        assertFalse(d2.newCard); assertTrue(d2.newHolo); assertEquals(2, d2.count)
        val d3 = a.add(1, holo = true, runId = 43, now = 3000L)!!
        assertFalse(d3.newHolo)
        val e = a[1]!!
        assertEquals(3, e.count); assertEquals(2, e.holoCount)
        assertEquals(42, e.firstSeenRun); assertEquals(1000L, e.firstSeenAt)
        assertEquals(1, a.collected); assertEquals(1, a.holoCollected)
    }

    @Test fun nonCardIdsAreIgnored() {
        val a = CardAlbum()
        assertNull(a.add(0, false, 1, 1))
        assertNull(a.add(MonsterFacts.COUNT + 1, false, 1, 1))
        assertNull(a.add(-3, true, 1, 1))
        assertEquals(0, a.collected)
    }

    @Test fun jsonRoundTrip() {
        val a = CardAlbum()
        a.add(5, false, 7, 111L)
        a.add(5, true, 8, 222L)
        a.add(MonsterFacts.LUCIFER, true, -12345, 1_700_000_000_000L)
        val b = CardAlbum.fromJson(a.toJson())
        assertEquals(a.toJson(), b.toJson())
        assertEquals(2, b.collected)
        val e = b[5]!!
        assertEquals(2, e.count); assertEquals(1, e.holoCount); assertEquals(7, e.firstSeenRun); assertEquals(111L, e.firstSeenAt)
        assertEquals(-12345, b[MonsterFacts.LUCIFER]!!.firstSeenRun)
        assertEquals(1_700_000_000_000L, b[MonsterFacts.LUCIFER]!!.firstSeenAt)
    }

    @Test fun badJsonNeverThrows() {
        assertEquals(0, CardAlbum.fromJson(null).collected)
        assertEquals(0, CardAlbum.fromJson("").collected)
        assertEquals(0, CardAlbum.fromJson("not json at all {{{").collected)
        // Unknown ids dropped, holo clamped to count, missing fields defaulted.
        val a = CardAlbum.fromJson("""[{"m":999,"n":1},{"m":3,"n":2,"h":9},{"m":4},{"x":1}]""")
        assertEquals(2, a.collected)
        assertEquals(2, a[3]!!.holoCount)
        assertEquals(1, a[4]!!.count)
        assertEquals(0, a[4]!!.holoCount)
    }

    @Test fun sixtyNineCardsAllWithLoreAndTiles() {
        assertEquals(69, MonsterFacts.COUNT)
        for (id in 1..MonsterFacts.COUNT) {
            val f = MonsterFacts.get(id)
            assertNotNull(f)
            assertEquals(id, f!!.id)
            assertTrue("lore for ${f.name}", f.lore.isNotBlank())
        }
        assertEquals(69, (1..69).map { MonsterFacts.get(it)!!.lore }.toSet().size)
        assertNull(MonsterFacts.get(0))
    }

    @Test fun tilesMatchMonsttilelist() {
        assertEquals(0, MonsterFacts.tileOf(1))                       // lemming
        assertEquals(55, MonsterFacts.tileOf(MonsterFacts.REDDRAGON))
        assertEquals(384, MonsterFacts.tileOf(MonsterFacts.NULLPOINTER))
        assertEquals(387, MonsterFacts.tileOf(MonsterFacts.ROOMBA))
        assertEquals(56, MonsterFacts.tileOf(MonsterFacts.DEMONLORD))
        assertEquals(56, MonsterFacts.tileOf(MonsterFacts.DEMONLORD + 1))
        assertEquals(62, MonsterFacts.tileOf(MonsterFacts.LUCIFER - 1))  // demon prince
        assertEquals(63, MonsterFacts.tileOf(MonsterFacts.LUCIFER))
    }

    @Test fun rarityMapping() {
        assertEquals(Rarity.COMMON, MonsterFacts.rarityOf(1))           // lemming, LV1
        assertEquals(Rarity.COMMON, MonsterFacts.rarityOf(17))          // zombie, LV3
        assertEquals(Rarity.UNCOMMON, MonsterFacts.rarityOf(18))        // assassin bug, LV4
        assertEquals(Rarity.UNCOMMON, MonsterFacts.rarityOf(33))        // forvalaka, LV6
        assertEquals(Rarity.RARE, MonsterFacts.rarityOf(34))            // lama nobe, LV7
        assertEquals(Rarity.RARE, MonsterFacts.rarityOf(52))            // spirit naga, LV10
        assertEquals(Rarity.EPIC, MonsterFacts.rarityOf(MonsterFacts.WHITEDRAGON))
        assertEquals(Rarity.EPIC, MonsterFacts.rarityOf(MonsterFacts.REDDRAGON))
        assertEquals(Rarity.EPIC, MonsterFacts.rarityOf(MonsterFacts.GNOMEKING))
        assertEquals(Rarity.EPIC, MonsterFacts.rarityOf(MonsterFacts.ROOMBA))
        assertEquals(Rarity.LEGENDARY, MonsterFacts.rarityOf(MonsterFacts.DEMONLORD))
        assertEquals(Rarity.LEGENDARY, MonsterFacts.rarityOf(MonsterFacts.LUCIFER))
        // Every rarity is reachable, and the filter covers each card exactly once.
        val a = CardAlbum()
        assertEquals(MonsterFacts.COUNT, Rarity.entries.sumOf { a.ids(it).size })
        for (r in Rarity.entries) assertTrue(r.label, a.ids(r).isNotEmpty())
    }

    @Test fun ownedFilter() {
        val a = CardAlbum()
        assertEquals(emptyList<Int>(), a.ids(null, Owned.FOUND))
        assertEquals(emptyList<Int>(), a.ids(null, Owned.HOLO))
        assertEquals(MonsterFacts.COUNT, a.ids(null, Owned.MISSING).size)
        a.add(17, false, 1, 1)                        // zombie, common
        a.add(1, true, 1, 1)                          // lemming, common, holo
        a.add(MonsterFacts.REDDRAGON, false, 1, 1)    // epic
        assertEquals(a.ids(null), a.ids(null, Owned.ALL))
        assertEquals(listOf(1, 17, MonsterFacts.REDDRAGON), a.ids(null, Owned.FOUND))   // monster order
        assertEquals(listOf(1), a.ids(null, Owned.HOLO))
        assertEquals(MonsterFacts.COUNT - 3, a.ids(null, Owned.MISSING).size)
        assertFalse(17 in a.ids(null, Owned.MISSING))
        // The two filters combine.
        assertEquals(listOf(1, 17), a.ids(Rarity.COMMON, Owned.FOUND))
        assertEquals(listOf(MonsterFacts.REDDRAGON), a.ids(Rarity.EPIC, Owned.FOUND))
        assertEquals(emptyList<Int>(), a.ids(Rarity.EPIC, Owned.HOLO))
        assertEquals(emptyList<Int>(), a.ids(Rarity.LEGENDARY, Owned.FOUND))
        // Found and missing split every rarity exactly.
        for (r in Rarity.entries)
            assertEquals(a.ids(r).size, a.ids(r, Owned.FOUND).size + a.ids(r, Owned.MISSING).size)
    }

    @Test fun summaryLine() {
        val a = CardAlbum()
        val n = MonsterFacts.COUNT
        assertEquals("0/$n collected · 0 holo", a.summary(null))
        assertEquals("no cards found yet", a.summary(null, Owned.FOUND))
        assertEquals("no holo cards yet", a.summary(null, Owned.HOLO))
        assertEquals("$n of $n still to find", a.summary(null, Owned.MISSING))
        a.add(17, false, 1, 1)
        a.add(1, true, 1, 1)
        a.add(MonsterFacts.REDDRAGON, false, 1, 1)
        val commons = a.ids(Rarity.COMMON).size
        assertEquals("3/$n collected · 1 holo", a.summary(null))
        assertEquals("3/$n collected", a.summary(null, Owned.FOUND))
        assertEquals("1 holo of 3 collected", a.summary(null, Owned.HOLO))
        assertEquals("${n - 3} of $n still to find", a.summary(null, Owned.MISSING))
        assertEquals("Common: 2/$commons collected", a.summary(Rarity.COMMON))
        assertEquals("Common: 1 holo of 2 collected", a.summary(Rarity.COMMON, Owned.HOLO))
        assertEquals("Legendary: no cards found yet", a.summary(Rarity.LEGENDARY, Owned.FOUND))
        for (id in 1..n) a.add(id, false, 1, 1)
        assertEquals("none left to find!", a.summary(null, Owned.MISSING))
        assertEquals("Epic: none left to find!", a.summary(Rarity.EPIC, Owned.MISSING))
    }

    @Test fun completeSet() {
        val a = CardAlbum()
        for (id in 1..MonsterFacts.COUNT) a.add(id, false, 1, 1)
        assertTrue(a.complete)
        assertEquals(MonsterFacts.COUNT, a.collected)
    }

    @Test fun titlesAndWrap() {
        assertEquals("Gnome King", MonsterFacts.get(MonsterFacts.GNOMEKING)!!.title)
        assertEquals("Type I Demon Lord", MonsterFacts.get(MonsterFacts.DEMONLORD)!!.title)
        assertEquals("God of Hellfire", MonsterFacts.get(MonsterFacts.LUCIFER)!!.title)
        assertEquals(listOf("aaa bb", "cccc"), wrap("aaa bb cccc", 6f) { it.length.toFloat() })
        assertEquals(listOf("toolongword"), wrap("toolongword", 3f) { it.length.toFloat() })
    }
}
