package com.ularn.android.audio

import com.ularn.android.audio.SfxDef.Seg
import com.ularn.android.audio.Wave.NOISE
import com.ularn.android.audio.Wave.PULSE12
import com.ularn.android.audio.Wave.PULSE25
import com.ularn.android.audio.Wave.PULSE50
import com.ularn.android.audio.Wave.TRIANGLE
import com.ularn.android.game.GameEvents

/** Dungeon Radio's sound effects: square-wave one-shots, each under 150 ms. */
object Sfx {
    val HIT = SfxDef("hit", listOf(Seg(NOISE, 1800.0, 500.0, 90, 0.9f), Seg(TRIANGLE, 110.0, 70.0, 40)))
    val KILL = SfxDef("kill", listOf(Seg(PULSE50, 520.0, 130.0, 70), Seg(NOISE, 3000.0, 800.0, 70, 0.8f)))
    val TAKE = SfxDef("take", listOf(Seg(PULSE25, 660.0, 660.0, 40), Seg(PULSE25, 990.0, 990.0, 60)))
    val GOLD = SfxDef("gold", listOf(Seg(PULSE50, 988.0, 988.0, 45, 0.8f), Seg(PULSE50, 1319.0, 1319.0, 90, 0.8f)))
    val STAIRS_DOWN = SfxDef("stairs down", listOf(Seg(TRIANGLE, 600.0, 150.0, 140)))
    val STAIRS_UP = SfxDef("stairs up", listOf(Seg(TRIANGLE, 150.0, 600.0, 140)))
    val SPELL = SfxDef("spell", listOf(Seg(PULSE12, 400.0, 1600.0, 120, 0.9f)))
    val LEVEL_UP = SfxDef("level up", listOf(
        Seg(PULSE25, 523.0, 523.0, 35), Seg(PULSE25, 659.0, 659.0, 35),
        Seg(PULSE25, 784.0, 784.0, 35), Seg(PULSE25, 1047.0, 1047.0, 40)))
    val CASSETTE = SfxDef("cassette", listOf(
        Seg(NOISE, 9000.0, 9000.0, 20, 0.6f), Seg(PULSE50, 784.0, 784.0, 50), Seg(PULSE50, 1047.0, 1047.0, 70)))
    val CARD = SfxDef("card", listOf(
        Seg(PULSE12, 1568.0, 1568.0, 40, 0.8f), Seg(PULSE12, 2093.0, 2093.0, 40, 0.8f), Seg(PULSE12, 2637.0, 2637.0, 50, 0.8f)))
    val CLUNK = SfxDef("clunk", listOf(Seg(TRIANGLE, 120.0, 60.0, 80), Seg(NOISE, 1500.0, 500.0, 60, 0.7f)))
    val SPLASH = SfxDef("splash", listOf(Seg(NOISE, 4000.0, 1200.0, 140, 0.8f)))
    val CHIRP = SfxDef("chirp", listOf(Seg(PULSE25, 1200.0, 1800.0, 60, 0.7f), Seg(PULSE25, 1500.0, 2200.0, 60, 0.7f)))
    val GLITCH = SfxDef("glitch", listOf(Seg(NOISE, 20000.0, 200.0, 60), Seg(PULSE12, 3000.0, 100.0, 80)))
    val POP = SfxDef("pop", listOf(Seg(PULSE50, 300.0, 900.0, 60, 0.8f)))

    val ALL = listOf(HIT, KILL, TAKE, GOLD, STAIRS_DOWN, STAIRS_UP, SPELL, LEVEL_UP, CASSETTE, CARD,
        CLUNK, SPLASH, CHIRP, GLITCH, POP)

    /**
     * The effect for a game event, or null for none. NEW_LEVEL is left to the
     * caller (up or down depends on the previous level); DIED / WON play a
     * stinger on the music bus instead.
     */
    fun forEvent(type: Int, a: Int): SfxDef? = when (type) {
        GameEvents.KILL -> KILL
        GameEvents.LEVEL_UP -> LEVEL_UP
        GameEvents.TAKE -> TAKE
        GameEvents.PLAYER_HIT -> HIT
        GameEvents.GOLD -> GOLD
        GameEvents.SPELL -> SPELL
        GameEvents.WILD_RADIO -> CASSETTE
        GameEvents.WILD_CARD -> CARD
        GameEvents.WILD_VENDING -> CLUNK
        GameEvents.WILD_HAT -> POP
        GameEvents.WILD_COIN -> if (a == 0) null else GOLD   // a = 0 is the silent price tick
        GameEvents.WILD_GLITCH -> GLITCH
        GameEvents.WILD_FISHING -> if (a == 3) GOLD else SPLASH   // a = 3 is a treasure map, read on dry land
        GameEvents.WILD_PET -> CHIRP
        GameEvents.WILD_PERK -> LEVEL_UP
        else -> null
    }
}
