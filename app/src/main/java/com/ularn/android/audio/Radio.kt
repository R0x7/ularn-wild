package com.ularn.android.audio

import android.app.Activity
import android.app.AlertDialog
import android.content.Context
import android.graphics.Typeface
import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import android.widget.Button
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.SeekBar
import android.widget.TextView
import android.widget.Toast
import com.ularn.android.Settings
import com.ularn.android.game.GameEvents
import com.ularn.android.game.GameState

/**
 * Dungeon Radio (Wild Edition, Settings.WILDF_RADIO): picks the music from
 * where the player is, plays a sound for each game event, keeps the cassette
 * tapes found so far, and shows the "Radio" dialog of the in-game menu.
 *
 * Register it with GameEvents; call [onResume] / [onPause] from the activity.
 * With the feature off it stays silent and stops its audio thread.
 */
class Radio(
    private val activity: Activity,
    private val settings: Settings,
    private val state: () -> GameState?,
    private val running: () -> Boolean,
) : GameEvents.Listener {
    private val engine = Chiptune()
    private val picker = TrackPicker()
    private val prefs = activity.getSharedPreferences("radio", Context.MODE_PRIVATE)
    private val main = Handler(Looper.getMainLooper())

    @Volatile private var resumed = false
    /** The player's pick in the Radio dialog; null = follow the dungeon. */
    private var chosen: Song? = null
    /** Level of the last NEW_LEVEL event, to tell stairs up from down. */
    @Volatile private var lastLevel = -1
    /** DIED / WON arrive before the game shows its final score. */
    @Volatile private var ended = 0
    private var wasRunning = false

    val enabled get() = settings.wild(Settings.WILDF_RADIO)

    /** Cassette tracks found (indices into [Songs.BONUS]); kept across games. */
    val unlocked: Set<Int>
        get() {
            val bits = prefs.getInt(KEY_UNLOCKED, 0)
            return Songs.BONUS.indices.filter { bits and (1 shl it) != 0 }.toSet()
        }

    private fun unlock(i: Int) {
        if (i !in Songs.BONUS.indices) return
        prefs.edit().putInt(KEY_UNLOCKED, prefs.getInt(KEY_UNLOCKED, 0) or (1 shl i)).apply()
    }

    // ------------------------------------------------------------ lifecycle

    fun onResume() {
        resumed = true
        applyVolumes()
        main.removeCallbacks(tick)
        main.post(tick)
    }

    fun onPause() {
        resumed = false
        main.removeCallbacks(tick)
        engine.stop()
    }

    private val tick = object : Runnable {
        override fun run() {
            update()
            if (resumed) main.postDelayed(this, TICK_MS)
        }
    }

    private fun applyVolumes() {
        engine.mixer.musicVolume = settings.radioMusicVolume / 100f
        engine.mixer.sfxVolume = settings.radioSfxVolume / 100f
    }

    /** Picks the track and starts or stops the audio thread. Main thread. */
    private fun update() {
        if (!enabled || !resumed) {
            engine.mixer.play(null)
            engine.stop()
            return
        }
        val now = running()
        if (now && !wasRunning) { ended = 0; lastLevel = -1 }   // a new game
        wasRunning = now
        val song = if (now) pick() else {
            // The game has exited: let a stinger play out, fade anything else.
            engine.mixer.song?.takeIf { !it.loop }
        }
        engine.mixer.play(song)
        if (!engine.mixer.silent) engine.start() else engine.stop()
    }

    private fun pick(): Song? {
        val s = state() ?: return null
        if (!s.refresh()) return null
        val scene = scene(s)
        if (scene.endState != 0) return picker.pick(scene, unlocked, SystemClock.uptimeMillis())
        val auto = picker.pick(scene, unlocked, SystemClock.uptimeMillis())
        return chosen ?: auto
    }

    private fun scene(s: GameState): TrackPicker.Scene {
        val text = s.displayMode == GameState.DISPLAY_TEXT
        var shop = false
        var bank = false
        if (text) {
            val sb = StringBuilder()
            for (row in 0 until s.textLines) {
                for (col in 0 until s.lineLen) sb.append(s.textChar(row, col))
                sb.append('\n')
            }
            val page = sb.toString()
            bank = TrackPicker.BANK_WORDS in page
            shop = TrackPicker.SHOP_WORDS.any { it in page }
        }
        var boss = false
        if (!text) {
            loop@ for (y in 0 until s.mapH) for (x in 0 until s.mapW) {
                if (s.tile(x, y) in DEMON_LORD_TILES) { boss = true; break@loop }
            }
        }
        return TrackPicker.Scene(
            dlevel = s.dlevel, textPage = text, shopText = shop, bankText = bank,
            bossVisible = boss, endState = if (s.endState != 0) s.endState else ended,
        )
    }

    // ------------------------------------------------------------- events

    /** From the game's notify thread. */
    override fun onEvent(e: GameEvents.Event) {
        if (!enabled) return
        val fx = when (e.type) {
            GameEvents.NEW_LEVEL -> {
                val prev = lastLevel
                lastLevel = e.a
                if (prev < 0 || prev == e.a) null else if (e.a < prev) Sfx.STAIRS_UP else Sfx.STAIRS_DOWN
            }
            else -> Sfx.forEvent(e.type, e.a)
        }
        when (e.type) {
            GameEvents.DIED -> { ended = TrackPicker.END_DIED; main.post { update() } }
            GameEvents.WON -> { ended = TrackPicker.END_WON; main.post { update() } }
            // a = 3 is a treasure map from a bottle: no water involved, so no surf track
            GameEvents.WILD_FISHING -> if (e.a != 3) main.post { picker.fishing(SystemClock.uptimeMillis()); update() }
            GameEvents.WILD_RADIO -> {
                unlock(e.a)
                val name = Songs.BONUS.getOrNull(e.a)?.name
                if (name != null) main.post {
                    Toast.makeText(activity, "New radio track: $name (menu → Radio)", Toast.LENGTH_LONG).show()
                }
            }
        }
        if (fx != null && resumed) {
            engine.mixer.sfx(fx)
            engine.start()
        }
    }

    // ------------------------------------------------------------- dialog

    /** The "Radio" entry of the in-game menu. */
    fun showDialog() {
        val density = activity.resources.displayMetrics.density
        fun dp(v: Int) = (v * density).toInt()
        val box = LinearLayout(activity).apply { orientation = LinearLayout.VERTICAL; setPadding(dp(20), dp(8), dp(20), 0) }
        fun header(t: String) = box.addView(TextView(activity).apply {
            text = t; setTypeface(typeface, Typeface.BOLD); setPadding(0, dp(14), 0, dp(2))
        })
        val playing = TextView(activity).apply { textSize = 16f }
        fun refresh() {
            val s = chosen ?: engine.mixer.song
            playing.text = "Now playing: " + (s?.name ?: "nothing") +
                if (chosen == null) "\n(follows the dungeon)" else "\n(your pick)"
        }
        fun choose(s: Song?) { chosen = s; update(); refresh() }
        box.addView(playing)
        box.addView(Button(activity).apply {
            text = "Next track"; isAllCaps = false
            setOnClickListener {
                val list = listOf<Song?>(null) + Songs.REGULAR + unlocked.sorted().map { Songs.BONUS[it] }
                val i = list.indexOf(chosen)
                choose(list[(i + 1) % list.size])
            }
        })
        box.addView(Button(activity).apply {
            text = "Follow the dungeon"; isAllCaps = false
            setOnClickListener { choose(null) }
        })

        header("Cassette tapes")
        val got = unlocked
        for ((i, song) in Songs.BONUS.withIndex()) {
            if (i in got) box.addView(Button(activity).apply {
                text = "▶ ${song.name}"; isAllCaps = false
                setOnClickListener { choose(song) }
            })
            else box.addView(TextView(activity).apply {
                text = "???  (a tape still hidden in the dungeon)"; alpha = 0.7f; setPadding(0, dp(6), 0, dp(6))
            })
        }

        header("Volume")
        fun slider(label: String, get: () -> Int, set: (Int) -> Unit, sample: () -> Unit) {
            val lbl = TextView(activity)
            fun show() { lbl.text = "$label: ${get()}%" }
            show()
            box.addView(lbl)
            box.addView(SeekBar(activity).apply {
                max = 100; progress = get()
                setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
                    override fun onProgressChanged(sb: SeekBar?, p: Int, user: Boolean) { if (user) { set(p); applyVolumes(); show() } }
                    override fun onStartTrackingTouch(sb: SeekBar?) {}
                    override fun onStopTrackingTouch(sb: SeekBar?) { sample() }
                })
            })
        }
        slider("Music", { settings.radioMusicVolume }, { settings.radioMusicVolume = it }) {}
        slider("Sound effects", { settings.radioSfxVolume }, { settings.radioSfxVolume = it }) {
            if (resumed && enabled) { engine.mixer.sfx(Sfx.GOLD); engine.start() }
        }
        box.addView(TextView(activity).apply {
            text = "Turn the radio off in Settings → Wild features."; textSize = 12f; alpha = 0.7f
            setPadding(0, dp(8), 0, 0)
        })

        refresh()
        AlertDialog.Builder(activity)
            .setTitle("Dungeon Radio")
            .setView(ScrollView(activity).apply { addView(box) })
            .setPositiveButton("Close", null)
            .show()
    }

    companion object {
        private const val KEY_UNLOCKED = "unlocked"
        private const val TICK_MS = 250L
        /** Demon lords and the Demon Prince (monster tiles 56..63). */
        private val DEMON_LORD_TILES = 56..63
    }
}
