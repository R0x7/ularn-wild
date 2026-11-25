package com.ularn.android.audio

import com.ularn.android.game.GameEvents
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import kotlin.math.abs

class SynthTest {
    private val rate = 8000

    @Test fun pulseDutyCycle() {
        for ((wave, duty) in listOf(Wave.PULSE12 to 0.125, Wave.PULSE25 to 0.25, Wave.PULSE50 to 0.5)) {
            val o = Osc(rate).apply { this.wave = wave; freq = 100.0 }
            val n = rate   // one second = 100 periods
            val high = (0 until n).count { o.next() > 0 }
            assertEquals("$wave", duty, high.toDouble() / n, 0.02)
        }
    }

    @Test fun triangleStaysInRange() {
        val o = Osc(rate).apply { wave = Wave.TRIANGLE; freq = 220.0 }
        val xs = (0 until rate).map { o.next() }
        assertTrue(xs.all { it in -1f..1f })
        assertTrue(xs.toSet().size in 8..17)   // a staircase, not a smooth ramp
    }

    @Test fun envelopeRisesAndReleases() {
        val e = Envelope(rate)
        assertTrue(e.idle)
        e.on(Adsr(0.01f, 0.01f, 0.5f, 0.01f))
        var peak = 0f
        repeat(rate / 10) { peak = maxOf(peak, e.next()) }
        assertEquals(1f, peak, 1e-3f)
        assertEquals(0.5f, e.next(), 1e-3f)   // sustaining
        e.off()
        repeat(rate / 10) { e.next() }
        assertTrue(e.idle)
    }

    @Test fun sequencerMakesSoundAndStingersEnd() {
        val seq = Sequencer(Songs.GAME_OVER, rate)
        val buf = FloatArray(rate)
        seq.render(buf, buf.size, 1f)
        assertTrue(buf.any { abs(it) > 0.1f })
        assertFalse(seq.finished)
        // Play past the end (plus release tails): it stops.
        repeat(((Songs.GAME_OVER.seconds) + 2).toInt()) { seq.render(buf, buf.size, 1f) }
        assertTrue(seq.finished)
        java.util.Arrays.fill(buf, 0f)
        seq.render(buf, buf.size, 1f)
        assertTrue(buf.all { it == 0f })
    }

    @Test fun loopingSongsKeepPlaying() {
        for (s in Songs.REGULAR + Songs.BONUS) {
            val seq = Sequencer(s, rate)
            val buf = FloatArray(rate)
            repeat(s.seconds.toInt() + 2) { java.util.Arrays.fill(buf, 0f); seq.render(buf, buf.size, 1f) }
            assertFalse(s.name, seq.finished)
            assertTrue(s.name, buf.any { abs(it) > 0.05f })
        }
    }

    @Test fun mixerCrossFadesAndStaysInRange() {
        val m = Mixer(rate)
        assertTrue(m.silent)
        m.play(Songs.TOWN)
        assertEquals(Songs.TOWN, m.song)
        val buf = FloatArray(512)
        m.render(buf, buf.size)
        // Fading in: the first buffer is quieter than one after the fade.
        val first = buf.maxOf { abs(it) }
        repeat(20) { m.render(buf, buf.size) }
        assertTrue(first <= buf.maxOf { abs(it) } + 1e-6f)
        m.play(Songs.TOWN)   // same song: no restart
        m.play(Songs.DEPTHS)
        assertEquals(Songs.DEPTHS, m.song)
        repeat(10) { Sfx.ALL.forEach { m.sfx(it) } }   // more than the polyphony
        repeat(50) {
            m.render(buf, buf.size)
            assertTrue(buf.all { it in -1f..1f })
        }
        m.play(null)
        repeat(50) { m.render(buf, buf.size) }
        assertTrue(m.silent)
    }

    @Test fun sfxAreShort() {
        for (d in Sfx.ALL) assertTrue("${d.name}: ${d.ms} ms", d.ms in 1..150)
    }

    @Test fun sfxForEvents() {
        assertNotNull(Sfx.forEvent(GameEvents.KILL, 3))
        assertNotNull(Sfx.forEvent(GameEvents.PLAYER_HIT, 5))
        assertNotNull(Sfx.forEvent(GameEvents.LEVEL_UP, 2))
        assertNotNull(Sfx.forEvent(GameEvents.WILD_RADIO, 0))
        assertNull(Sfx.forEvent(GameEvents.NEW_LEVEL, 1))   // the caller picks up / down
        assertNull(Sfx.forEvent(GameEvents.WILD_COIN, 0))   // the silent price tick
        assertEquals(Sfx.GOLD, Sfx.forEvent(GameEvents.WILD_WEATHER, 0, 2))   // a rainbow's pot of gold
        assertEquals(Sfx.GOLD, Sfx.forEvent(GameEvents.WILD_WEATHER, 0, 3))   // a shooting star's gem
        assertNull(Sfx.forEvent(GameEvents.WILD_WEATHER, 0, 0))   // a plain change of weather
        assertNull(Sfx.forEvent(GameEvents.WILD_WEATHER, 3, 1))   // lightning: the screen flashes instead
        assertNull(Sfx.forEvent(9999, 0))
    }
}
