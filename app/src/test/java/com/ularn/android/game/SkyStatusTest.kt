package com.ularn.android.game

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class SkyStatusTest {
    private fun sky(t: Int, w: Int = WeatherMath.CLEAR, outside: Boolean = true) = SkyStatus(t, w, outside)

    @Test fun clockMatchesTheGame() {
        for (t in listOf(0, 49, 50, 1199, 1200, 1500, 123_457)) {
            assertEquals("turn $t", WeatherMath.hourAt(t.toLong()), sky(t).hour)
        }
        assertEquals("Day 1, 08:00 · Clear", sky(0).title)
        assertEquals("Day 1, 08:01 · Clear", sky(1).title)          // a turn is 1.2 minutes
        assertEquals("Day 1, 08:58 · Clear", sky(49).title)
        assertEquals("Day 1, 14:24 · Rain", sky(320, WeatherMath.RAIN).title)
    }

    @Test fun aNewDayStartsAtMidnight() {
        assertEquals(1, sky(799).day)       // 23:58
        assertEquals(2, sky(800).day)       // 00:00
        assertEquals("Day 2, 00:00 · Clear night", sky(800).title)
        assertEquals(2, sky(800 + 1199).day)
        assertEquals(3, sky(2000).day)
    }

    @Test fun weatherOnlyShowsUnderTheSky() {
        assertEquals("Day 1, 14:24 · Snow", sky(320, WeatherMath.SNOW).title)
        assertEquals("Day 1, 14:24", sky(320, WeatherMath.SNOW, outside = false).title)
        assertEquals("Day 1, 14:24 · Storm", sky(320, WeatherMath.STORM).title)
    }

    @Test fun countsDownToDuskAndDawn() {
        // 08:00: night falls at 21:00, 13 hours away
        assertFalse(sky(0).isNight)
        assertEquals("Night falls in 650 turns", sky(0).outlook)
        assertEquals("Night falls in 1 turn", sky(649).outlook)
        // 21:00: night until 06:00
        assertTrue(sky(650).isNight)
        assertEquals("Dawn breaks in 450 turns", sky(650).outlook)
        assertEquals("Dawn breaks in 1 turn", sky(1099).outlook)
        assertFalse(sky(1100).isNight)
        assertEquals("Night falls in 750 turns", sky(1100).outlook)
    }

    @Test fun theCountdownAlwaysLandsOnTheChange() {
        for (t in 0 until 2400) {
            val s = sky(t)
            val n = s.turnsToChange
            assertTrue("turn $t: $n", n in 1..WeatherMath.TURNS_PER_HOUR * 15)
            assertEquals("turn $t", !s.isNight, sky(t + n).isNight)
            assertEquals("turn $t", s.isNight, sky(t + n - 1).isNight)
        }
    }
}
