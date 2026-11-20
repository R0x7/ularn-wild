package com.ularn.android.game

import java.nio.ByteBuffer
import java.nio.ByteOrder

/**
 * A consistent copy of the game's display state (see cpp/shared_state.h).
 *
 * The game process writes the shared struct; [refresh] copies the whole thing
 * (about 10 KB) into a local array once per frame, and the accessors read
 * from that copy. Offsets come from the native side, so nothing here has to
 * be kept in sync with the C struct by hand.
 */
class GameState(private val shared: ByteBuffer, layout: IntArray) {

    /** Indices into NativeBridge.layout(); order matches ularn_jni.c. */
    private object F {
        const val SIZE = 0; const val MAGIC = 1; const val SEQ = 2
        const val DISPLAY = 3; const val INPUT = 4; const val ANSWERS = 5
        const val CARET = 6; const val CX = 7; const val CY = 8; const val BEEP = 9
        const val PX = 10; const val PY = 11; const val PCURSOR = 12
        const val TILES = 13; const val OVERLAY = 14
        const val STATUS1 = 15; const val STATUS2 = 16
        const val HP = 17; const val HPMAX = 18; const val SPELLS = 19; const val SPELLMAX = 20
        const val GOLD = 21; const val PLEVEL = 22; const val AC = 23; const val WC = 24
        const val EXP = 25; const val DLEVEL = 26; const val LEVELNAME = 27
        const val EFFECTS = 28; const val CLASS = 29; const val SEX = 30
        const val STATS = 31; const val LEVEL_TITLE = 32; const val GAME_TIME = 33; const val HARDNESS = 34
        const val MSG_CHR = 35; const val MSG_FMT = 36; const val TEXT_CHR = 37; const val TEXT_FMT = 38
        const val MAP_W = 39; const val MAP_H = 40; const val LINE_LEN = 41
        const val MSG_LINES = 42; const val TEXT_LINES = 43
        // minimap / side panel
        const val MM_CAT = 44; const val INV_TEXT = 45; const val INV_SLOTS = 46; const val INV_LEN = 47
        // context action / spells
        const val CTX_OBJECT = 48; const val CTX_KIND = 49; const val CTX_FLAGS = 50; const val CTX_DOORS = 51
        const val CTX_NAME = 52; const val SPELL_COUNT = 53; const val SPELL_KNOWN = 54; const val SPELL_CODES = 55
        const val CTX_NAME_LEN = 56; const val SPELLS_MAX = 57
        // danger cues / stats
        const val RUN_ID = 58; const val KILLS = 59; const val END_STATE = 60
        const val END_SCORE = 61; const val END_REASON = 62
        // Wild Edition (shared_state.h, wild_paint())
        const val WILD_FLAGS = 63; const val HAT_TILE = 64; const val GLITCH = 65; const val WEATHER = 66
        const val COIN_PRICE = 67; const val PET_X = 68; const val PET_Y = 69
        const val KILL_COUNTS = 70; const val KILL_COUNTS_N = 71
        const val EV_SEQ = 72; const val EV = 73; const val EVENTS_N = 74
        const val PET_INFO = 75; const val PET_LEVEL = 76
        const val BOUNTY_INFO = 77
    }

    private val off = layout
    private val snap = ByteArray(layout[F.SIZE])
    private val buf: ByteBuffer = ByteBuffer.wrap(snap).order(ByteOrder.nativeOrder())
    private val live: ByteBuffer = shared.duplicate().order(ByteOrder.nativeOrder())

    val mapW = layout[F.MAP_W]
    val mapH = layout[F.MAP_H]
    val lineLen = layout[F.LINE_LEN]
    val msgLines = layout[F.MSG_LINES]
    val textLines = layout[F.TEXT_LINES]

    /** Cheap check for "anything changed?" without copying. */
    fun liveSeq(): Int = live.getInt(off[F.SEQ])

    /** Copies the shared struct. Returns false if it isn't initialised yet. */
    fun refresh(): Boolean {
        val src = live.duplicate()
        src.position(0)
        src.get(snap, 0, snap.size)
        return buf.getInt(off[F.MAGIC]) == MAGIC
    }

    private fun i(f: Int) = buf.getInt(off[f])

    val seq get() = i(F.SEQ)
    val displayMode get() = i(F.DISPLAY)
    val inputMode get() = i(F.INPUT)
    val caret get() = i(F.CARET) != 0
    val cursorX get() = i(F.CX)
    val cursorY get() = i(F.CY)
    val beepCount get() = i(F.BEEP)
    val playerX get() = i(F.PX)
    val playerY get() = i(F.PY)
    val playerCursor get() = i(F.PCURSOR) != 0
    val hp get() = i(F.HP)
    val hpMax get() = i(F.HPMAX)
    val spells get() = i(F.SPELLS)
    val spellMax get() = i(F.SPELLMAX)
    val gold get() = i(F.GOLD)
    val playerLevel get() = i(F.PLEVEL)
    val effects get() = i(F.EFFECTS)
    val classNum get() = i(F.CLASS)
    val gameTime get() = i(F.GAME_TIME)
    val hardness get() = i(F.HARDNESS)
    val ac get() = i(F.AC)
    val wc get() = i(F.WC)
    val exp get() = i(F.EXP)
    val dlevel get() = i(F.DLEVEL)

    // danger cues / stats
    /** Random id of the current character (kept in its save); 0 = none yet. */
    val runId get() = i(F.RUN_ID)
    /** Monsters the player has killed this run. */
    val kills get() = i(F.KILLS)
    /** One of END_*: set when the game shows the final score. */
    val endState get() = i(F.END_STATE)
    val endScore get() = i(F.END_SCORE)
    val endReason: String get() = cString(off[F.END_REASON], lineLen + 1).trim()

    // ---- Wild Edition ----
    /** Low 16 bits: feature mask (Settings.WILDF_*); high 16 bits: perk bits. */
    val wildFlags get() = i(F.WILD_FLAGS)
    fun wildOn(bit: Int) = (wildFlags and bit) != 0
    val perkBits get() = (wildFlags ushr 16) and 0xffff
    /** Overlay tile to draw on the player (a hat), 0 = none. */
    val hatTile get() = i(F.HAT_TILE)
    /** Turns of glitch left; 0 = reality is fine. */
    val glitch get() = i(F.GLITCH)
    /** Surface weather: hour (0-23) in the low byte, weather state in the next. */
    val weatherHour get() = i(F.WEATHER) and 0xff
    val weatherState get() = (i(F.WEATHER) ushr 8) and 0xff
    val coinPrice get() = i(F.COIN_PRICE)
    /** The pet's cell on this level, or -1. */
    val petX get() = i(F.PET_X)
    val petY get() = i(F.PET_Y)
    /** The pet as the side panel shows it (shared_state.h: pet_info, pet_level), or null without one. */
    val pet: PetStatus? get() {
        fun info(n: Int) = buf.getInt(off[F.PET_INFO] + 4 * n)
        if (info(0) <= 0) return null
        return PetStatus(info(0), info(1), info(2), info(3), info(4), info(5), cString(off[F.PET_LEVEL], 8).trim())
    }
    /** The town crier's WANTED poster (shared_state.h: bounty_info), or null without one. */
    val bounty: BountyStatus? get() {
        fun info(n: Int) = buf.getInt(off[F.BOUNTY_INFO] + 4 * n)
        if (info(0) <= 0) return null
        return BountyStatus(info(0), info(1), info(2) != 0, info(3))
    }
    val killCountSlots get() = off[F.KILL_COUNTS_N]
    /** Monsters of species [monst] this character has killed (saved with the game). */
    fun killCount(monst: Int): Int =
        if (monst in 0 until killCountSlots) buf.getInt(off[F.KILL_COUNTS] + 4 * monst) else 0
    /** Event ring (see GameEvents): the live sequence number and one entry. */
    val eventSlots get() = off[F.EVENTS_N]
    fun liveEventSeq(): Int = live.getInt(off[F.EV_SEQ])
    fun liveEvent(index: Int, field: Int): Int = live.getInt(off[F.EV] + 16 * (index and (eventSlots - 1)) + 4 * field)

    /** STR (with bonus), INT, WIS, CON, DEX, CHA. */
    fun stat(i: Int): Int = buf.getInt(off[F.STATS] + 4 * i)
    val levelTitle: String get() = cString(off[F.LEVEL_TITLE], 40)

    val answers: String get() = cString(off[F.ANSWERS], 64)
    val status1: String get() = cString(off[F.STATUS1], 96)
    val status2: String get() = cString(off[F.STATUS2], 96)
    val levelName: String get() = cString(off[F.LEVELNAME], 8).trim()

    fun tile(x: Int, y: Int): Int = buf.getShort(off[F.TILES] + 2 * (y * mapW + x)).toInt()
    fun overlay(x: Int, y: Int): Int = buf.getShort(off[F.OVERLAY] + 2 * (y * mapW + x)).toInt()

    /** Character at (col, row) of the message area (map mode). */
    fun msgChar(row: Int, col: Int): Char = (snap[off[F.MSG_CHR] + row * (lineLen + 1) + col].toInt() and 0xff).toChar()
    fun msgFmt(row: Int, col: Int): Int = snap[off[F.MSG_FMT] + row * lineLen + col].toInt() and 0xff

    /** Character at (col, row) of the full text page (text mode). */
    fun textChar(row: Int, col: Int): Char = (snap[off[F.TEXT_CHR] + row * (lineLen + 1) + col].toInt() and 0xff).toChar()
    fun textFmt(row: Int, col: Int): Int = snap[off[F.TEXT_FMT] + row * lineLen + col].toInt() and 0xff

    // minimap / side panel
    val invSlots get() = off[F.INV_SLOTS]

    /** Minimap class of a square: low 5 bits MM_*, plus [MM_MONSTER]. */
    fun mmCat(x: Int, y: Int): Int = snap[off[F.MM_CAT] + y * mapW + x].toInt() and 0xff

    /** Inventory slot [i] ('a' + i) as the game words it; "" if empty. */
    fun invText(i: Int): String = cString(off[F.INV_TEXT] + i * off[F.INV_LEN], off[F.INV_LEN])
    // context action / spells (see ContextAction.kt, SpellPicker.kt)
    val ctxObject get() = i(F.CTX_OBJECT)
    val ctxKind get() = i(F.CTX_KIND)
    val ctxFlags get() = i(F.CTX_FLAGS)
    val ctxDoors get() = i(F.CTX_DOORS)
    val ctxName: String get() = cString(off[F.CTX_NAME], off[F.CTX_NAME_LEN])
    val spellCount get() = i(F.SPELL_COUNT).coerceIn(0, off[F.SPELLS_MAX])
    fun spellKnown(n: Int): Boolean = n in 0 until spellCount && snap[off[F.SPELL_KNOWN] + n].toInt() != 0
    fun spellCode(n: Int): String = if (n in 0 until spellCount) cString(off[F.SPELL_CODES] + 4 * n, 4) else ""

    private fun cString(at: Int, max: Int): String {
        var n = 0
        while (n < max && snap[at + n].toInt() != 0) n++
        return String(snap, at, n, Charsets.ISO_8859_1)
    }

    companion object {
        const val MAGIC = 0x554C524E

        const val DISPLAY_MAP = 0
        const val DISPLAY_TEXT = 1

        const val INPUT_BUSY = 0
        const val INPUT_COMMAND = 1
        const val INPUT_PROMPT = 2
        const val INPUT_DIR = 3
        const val INPUT_NUMBER = 4
        const val INPUT_PASSWORD = 5

        const val FMT_NORMAL = 0
        const val FMT_STANDOUT = 1
        const val FMT_STANDOUT2 = 2
        const val FMT_STANDOUT3 = 3
        const val FMT_BOLD = 4
        const val FMT_INVERSE = 5

        const val END_NONE = 0
        const val END_DIED = 1
        const val END_WON = 2
        const val END_QUIT = 3

        const val TILE_CURSOR = 174

        // Minimap classes (US_MM_* in shared_state.h)
        const val MM_UNKNOWN = 0; const val MM_FLOOR = 1; const val MM_WALL = 2; const val MM_DOOR = 3
        const val MM_ITEM = 4; const val MM_GOLD = 5; const val MM_UP = 6; const val MM_DOWN = 7
        const val MM_VOLCANO = 8; const val MM_ENTRANCE = 9; const val MM_BUILDING = 10
        const val MM_FOUNTAIN = 11; const val MM_ALTAR = 12; const val MM_THRONE = 13
        const val MM_TRAP = 14; const val MM_FEATURE = 15
        const val MM_CLASS_MASK = 0x1f
        const val MM_MONSTER = 0x80

        val STAT_NAMES = listOf("STR", "INT", "WIS", "CON", "DEX", "CHA")

        /** Bit i of [effects], in the order the front end reports them. */
        val EFFECT_NAMES = listOf(
            "Stealth", "Undead Pro", "Spirit Pro", "Charm", "Time Stop", "Hold Monst",
            "Giant Str", "Fire Resist", "Dexterity", "Strength", "Scare", "Haste Self",
            "Cancel", "Invisible", "Protect 3", "Protect 2", "Wall-Walk",
        )
    }
}
