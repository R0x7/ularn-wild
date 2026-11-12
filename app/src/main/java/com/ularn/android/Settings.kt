package com.ularn.android

import android.content.Context
import android.content.SharedPreferences
import kotlin.properties.ReadWriteProperty
import kotlin.reflect.KProperty

/** App + game preferences. */
class Settings(context: Context) {
    private val p = context.getSharedPreferences("settings", Context.MODE_PRIVATE)

    // Each preference is a property backed by one SharedPreferences key;
    // writes are applied asynchronously, as before.
    private fun bool(key: String, default: Boolean) =
        pref({ getBoolean(key, default) }, { putBoolean(key, it) })
    private fun int(key: String, default: Int) =
        pref({ getInt(key, default) }, { putInt(key, it) })
    private fun float(key: String, default: Float) =
        pref({ getFloat(key, default) }, { putFloat(key, it) })
    private fun string(key: String, default: String) =
        pref({ getString(key, default) ?: default }, { putString(key, it) })

    private fun <T> pref(
        read: SharedPreferences.() -> T,
        write: SharedPreferences.Editor.(T) -> Unit,
    ) = object : ReadWriteProperty<Any?, T> {
        override fun getValue(thisRef: Any?, property: KProperty<*>): T = p.read()
        override fun setValue(thisRef: Any?, property: KProperty<*>, value: T) =
            p.edit().apply { write(value) }.apply()
    }

    var playerName by string("name", "Adventurer")

    var female by bool("female", false)

    /** Difficulty for the next new game (the game's `-d` option); see [MAX_DIFFICULTY]. */
    var difficulty: Int
        get() = storedDifficulty.coerceIn(0, MAX_DIFFICULTY)
        set(v) { storedDifficulty = v.coerceIn(0, MAX_DIFFICULTY) }
    private var storedDifficulty by int("difficulty", 0)

    var skipIntro by bool("skipIntro", false)

    var noNap by bool("noNap", false)

    var noBeep by bool("noBeep", false)

    /**
     * Ularn 1.6's "enhanced interface": doors open with 'o' + direction
     * instead of by walking into them, and 'O' opens a chest you stand on.
     */
    var enhancedInterface by bool("enhanced", false)

    /** Tap a square on the map to step towards it (long-press to run). */
    var tapToMove by bool("tapMove", true)

    /** Swipe on the map to step; hold to keep walking. */
    var swipeToMove by bool("swipeMove", true)

    /** A quick flick on the map runs instead of stepping. */
    var flickToRun by bool("flickRun", false)

    /** Stop the screen from dimming and locking while the app is open. */
    var keepScreenOn by bool("keepScreenOn", true)

    /** See [ORIENTATIONS]. */
    var orientation by int("orientation", 0)

    /** Landscape: keep the map clear of the side controls. */
    var fitMapBesideControls by bool("fitBeside", true)

    /** Hardware/Bluetooth keyboard arrow keys map to hjkl. */
    var arrowsAsMoves by bool("arrows", true)

    /** Round minimap in a corner of the map (tap it for the whole level). */
    var showMinimap by bool("minimap", true)

    /** Wide windows (tablets, foldables): stats / map / inventory panel beside the map. */
    var sidePanel by bool("sidePanel", true)

    /** Map zoom, as tile size in dp; 0 = automatic. */
    var tileDp by float("tileDp", 0f)

    // danger cues / stats
    /** Low-HP cues start below this percentage of max HP. */
    var dangerThresholdPct by int("dangerPct", 25)

    /** Low HP: the HP bar pulses red. */
    var dangerPulse by bool("dangerPulse", true)

    /** Low HP: a red vignette around the map. */
    var dangerVignette by bool("dangerVignette", true)

    /** Vibrate once when HP falls below the threshold. */
    var dangerVibrate by bool("dangerVibrate", true)

    /** Red flash (and a short buzz) when one hit takes 20% of max HP or more. */
    var bigHitCue by bool("bigHitCue", true)

    /** Show a toast when an achievement unlocks. */
    var achievementToasts by bool("achToasts", true)

    // ---- Wild Edition ----
    /**
     * Feature mask passed to the game as ULARN_WILD (WILDF_* bits); default all on.
     * "wildKnown" records which bits existed when the mask was last saved, so a
     * feature added in a later version starts switched on instead of reading as
     * an unticked box (masks saved before it existed covered 0x7ff).
     */
    var wildFlags: Int
        get() {
            val known = p.getInt("wildKnown", 0x7ff)
            return (p.getInt("wildFlags", WILDF_ALL) or (WILDF_ALL and known.inv())) and WILDF_ALL
        }
        set(v) = p.edit().putInt("wildFlags", v and WILDF_ALL).putInt("wildKnown", WILDF_ALL).apply()

    /** Trading cards: pop up the card when one drops. */
    var cardToasts: Boolean
        get() = p.getBoolean("cardToasts", true)
        set(v) = p.edit().putBoolean("cardToasts", v).apply()

    /** Glitch Realm: no map slicing, hue cycling or animation (motion-sensitive players). */
    var reduceGlitch: Boolean
        get() = p.getBoolean("reduceGlitch", false)
        set(v) = p.edit().putBoolean("reduceGlitch", v).apply()

    /** Weather: no drifting rain / snow and no lightning flashes (the day/night tint stays). */
    var reduceWeather: Boolean
        get() = p.getBoolean("reduceWeather", false)
        set(v) = p.edit().putBoolean("reduceWeather", v).apply()

    /** Photo mode: snap a Polaroid by itself when the character dies or wins. */
    var autoPhoto: Boolean
        get() = p.getBoolean("autoPhoto", true)
        set(v) = p.edit().putBoolean("autoPhoto", v).apply()

    fun wild(bit: Int) = (wildFlags and bit) != 0
    fun setWild(bit: Int, on: Boolean) { wildFlags = if (on) wildFlags or bit else wildFlags and bit.inv() }

    /** Dungeon Radio: music volume, 0..100 (Radio entry of the in-game menu). */
    var radioMusicVolume: Int
        get() = p.getInt("radioMusic", 60)
        set(v) = p.edit().putInt("radioMusic", v.coerceIn(0, 100)).apply()

    /** Dungeon Radio: sound effects volume, 0..100. */
    var radioSfxVolume: Int
        get() = p.getInt("radioSfx", 80)
        set(v) = p.edit().putInt("radioSfx", v.coerceIn(0, 100)).apply()

    companion object {
        // Feature bits: keep in sync with compat/wild.h (WILDF_*).
        const val WILDF_RADIO = 1 shl 0
        const val WILDF_CARDS = 1 shl 1
        const val WILDF_VENDING = 1 shl 2
        const val WILDF_HATS = 1 shl 3
        const val WILDF_COIN = 1 shl 4
        const val WILDF_GLITCH = 1 shl 5
        const val WILDF_FISHING = 1 shl 6
        const val WILDF_PETS = 1 shl 7
        const val WILDF_PERKS = 1 shl 8
        const val WILDF_WEATHER = 1 shl 9
        const val WILDF_PHOTO = 1 shl 10
        const val WILDF_BOUNTY = 1 shl 11
        const val WILDF_ALL = 0xfff

        /** Checkbox labels for the Settings "Wild features" section, in bit order. */
        val WILD_FEATURES = listOf(
            WILDF_RADIO to "Dungeon Radio: chiptune music and sound effects",
            WILDF_CARDS to "Monster trading cards drop from kills",
            WILDF_VENDING to "Vending machines, junk food, the rubber duck and the Roomba",
            WILDF_HATS to "Hats",
            WILDF_COIN to "LarnCoin at the bank (and the Tax Auditor)",
            WILDF_GLITCH to "The Glitch Realm (scroll of segfault)",
            WILDF_FISHING to "Fishing at fountains",
            WILDF_PETS to "Mysterious eggs hatch into pets",
            WILDF_PERKS to "Choose a perk on every level-up",
            WILDF_WEATHER to "Weather and day/night on the surface",
            WILDF_PHOTO to "Polaroid photo mode",
            WILDF_BOUNTY to "Town bounty board: WANTED posters with gold rewards",
        )

        /** Highest difficulty the slider offers (the game itself accepts up to 100). */
        const val MAX_DIFFICULTY = 10

        /** Game arguments for a new game at difficulty [level]. */
        fun difficultyArgs(level: Int): List<String> =
            if (level > 0) listOf("-d", level.coerceAtMost(MAX_DIFFICULTY).toString()) else emptyList()

        /**
         * What [level] does to monsters, mirroring the game's sethard():
         * hit points x(6+k)/6, damage x(6+k)/5, gold x10/(10+k),
         * armour class -k, experience x7/(7+k).
         */
        fun describeDifficulty(level: Int): String {
            val k = level.coerceIn(0, MAX_DIFFICULTY)
            if (k == 0) return "Standard game: monsters at their normal strength."
            fun pct(f: Double) = Math.round(f * 100).toInt()
            return "Monsters: ${pct((6 + k) / 6.0)}% hit points, ${pct((6 + k) / 5.0)}% damage, " +
                "armour class -$k. They drop ${pct(10.0 / (10 + k))}% gold and give " +
                "${pct(7.0 / (7 + k))}% experience."
        }

        /** Rotation choices: label to ActivityInfo.screenOrientation. */
        val ORIENTATIONS = listOf(
            "Follow system rotation setting" to android.content.pm.ActivityInfo.SCREEN_ORIENTATION_FULL_USER,
            "Always auto-rotate" to android.content.pm.ActivityInfo.SCREEN_ORIENTATION_FULL_SENSOR,
            "Portrait" to android.content.pm.ActivityInfo.SCREEN_ORIENTATION_SENSOR_PORTRAIT,
            "Landscape" to android.content.pm.ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE,
        )
        const val ORIENT_PORTRAIT = 2
        const val ORIENT_LANDSCAPE = 3
    }

    /** Builds Ularn 1.6.3a's ularn.opt: "OPTION name:X,gender:male,...". */
    fun buildOptsFile(playerName: String = this.playerName, female: Boolean = this.female): String {
        // ',' and ':' separate options in ularn.opt.
        val name = playerName.replace(Regex("[,:\\r\\n]"), " ").trim().take(30).ifBlank { "Adventurer" }
        val opts = mutableListOf("name:$name", "gender:" + if (female) "female" else "male")
        if (skipIntro) opts += "nowelcome"
        if (noNap) opts += "nonap"
        if (noBeep) opts += "nobeep"
        opts += if (enhancedInterface) "enhanced_interface" else "noenhanced_interface"
        return "# Written by the Ularn app; edit in Settings.\nOPTION " + opts.joinToString(",") + "\n"
    }
}
