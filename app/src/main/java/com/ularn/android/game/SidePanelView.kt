package com.ularn.android.game

import android.annotation.SuppressLint
import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.RectF
import android.graphics.Typeface
import android.view.MotionEvent
import android.view.View
import android.view.ViewConfiguration
import kotlin.math.abs

/**
 * The tablet / foldable side panel: character stats and active effects, the
 * whole-level overview (tap it for the full-screen version with a legend),
 * and the inventory, all read live from [GameState]. Drag to scroll when it
 * doesn't fit.
 */
class SidePanelView(context: Context, private val stateOf: () -> GameState?) : View(context) {

    private val density = resources.displayMetrics.density
    private fun dp(v: Float) = v * density

    private val renderer = MinimapRenderer(density)
    private val ui = Paint(Paint.ANTI_ALIAS_FLAG).apply { typeface = Typeface.SANS_SERIF }
    private val bold = Paint(Paint.ANTI_ALIAS_FLAG).apply { typeface = Typeface.create(Typeface.SANS_SERIF, Typeface.BOLD) }
    private val fill = Paint(Paint.ANTI_ALIAS_FLAG)

    private val bg = Color.rgb(18, 19, 23)
    private val line = Color.rgb(44, 46, 54)
    private val fg = Color.rgb(222, 222, 216)
    private val dim = Color.rgb(150, 150, 150)
    private val accent = Color.rgb(255, 190, 60)

    private var scroll = 0f
    private var contentH = 0f
    private val mapBox = RectF()
    private var mapShown = false

    override fun onDraw(canvas: Canvas) {
        canvas.drawColor(bg)
        fill.color = line
        canvas.drawRect(0f, 0f, dp(1f), height.toFloat(), fill)

        val pad = dp(12f)
        val x0 = pad + dp(1f)
        val w = width - x0 - pad
        mapShown = false
        val s = stateOf()
        if (s == null || !s.refresh() || s.seq == 0 || s.classNum < 0 || w < dp(60f)) {
            ui.textSize = dp(13f); ui.color = dim
            canvas.drawText("No character yet", x0, pad - ui.ascent(), ui)
            contentH = 0f; scroll = 0f
            return
        }

        scroll = scroll.coerceIn(0f, maxOf(0f, contentH - height))
        canvas.save()
        canvas.translate(0f, -scroll)
        var y = pad

        // --- character
        y = header(canvas, "CHARACTER", x0, y, w)
        bold.textSize = dp(14f); bold.color = Color.WHITE
        y = text(canvas, "Level ${s.playerLevel} ${s.levelTitle}", x0, y, w, bold)
        ui.textSize = dp(13f)
        ui.color = fg
        val where = if (s.levelName == "H") "Home" else "Dungeon level ${s.levelName}"
        y = text(canvas, where, x0, y, w, ui)
        y = bar(canvas, "HP", s.hp, s.hpMax, x0, y, w, hpColor(s.hp, s.hpMax))
        y = bar(canvas, "Spells", s.spells, s.spellMax, x0, y, w, Color.rgb(110, 150, 255))
        ui.color = fg
        y = text(canvas, "AC ${s.ac}   WC ${s.wc}   Exp ${s.exp}", x0, y, w, ui)
        ui.color = accent
        y = text(canvas, "Gold ${s.gold}", x0, y, w, ui)
        // stats: two columns of three
        ui.color = fg
        val colW = w / 2
        for (row in 0 until 3) {
            for (col in 0 until 2) {
                val i = col * 3 + row
                drawLabelValue(canvas, GameState.STAT_NAMES[i], s.stat(i).toString(), x0 + col * colW, y, colW - dp(8f))
            }
            y += rowH()
        }
        ui.color = dim
        y = text(canvas, "Turn ${s.gameTime}", x0, y, w, ui)

        // --- effects
        val active = GameState.EFFECT_NAMES.filterIndexed { i, _ -> s.effects and (1 shl i) != 0 }
        if (active.isNotEmpty()) {
            y += dp(4f)
            ui.textSize = dp(11f)
            val h = dp(20f)
            var cx = x0
            for (name in active) {
                val cw = ui.measureText(name) + dp(12f)
                if (cx > x0 && cx + cw > x0 + w) { cx = x0; y += h + dp(4f) }
                fill.color = Color.argb(200, 60, 50, 20)
                canvas.drawRoundRect(cx, y, cx + cw, y + h, h / 2, h / 2, fill)
                ui.color = accent
                canvas.drawText(name, cx + dp(6f), y + h / 2 - (ui.descent() + ui.ascent()) / 2, ui)
                cx += cw + dp(4f)
            }
            y += h + dp(4f)
        }

        // --- level-up perks (Wild Edition; bits from compat/wild_perks.c)
        val perks = PERK_NAMES.filterIndexed { i, _ -> s.perkBits and (1 shl i) != 0 }
        if (perks.isNotEmpty()) {
            y += dp(8f)
            y = header(canvas, "PERKS", x0, y, w)
            ui.textSize = dp(11f)
            val h = dp(20f)
            var cx = x0
            for (name in perks) {
                val cw = ui.measureText(name) + dp(12f)
                if (cx > x0 && cx + cw > x0 + w) { cx = x0; y += h + dp(4f) }
                fill.color = Color.argb(200, 30, 50, 70)
                canvas.drawRoundRect(cx, y, cx + cw, y + h, h / 2, h / 2, fill)
                ui.color = Color.rgb(140, 200, 255)
                canvas.drawText(name, cx + dp(6f), y + h / 2 - (ui.descent() + ui.ascent()) / 2, ui)
                cx += cw + dp(4f)
            }
            y += h + dp(4f)
            ui.textSize = dp(13f)
        }

        // --- the pet (Wild Edition; compat/wild_pets.c)
        s.pet?.let { pet ->
            y += dp(8f)
            y = header(canvas, "PET", x0, y, w)
            bold.textSize = dp(13f); bold.color = Color.WHITE
            y = text(canvas, pet.title, x0, y, w, bold)
            ui.textSize = dp(13f)   // the effect and perk chips leave it at 11
            y = bar(canvas, "HP", pet.hp, pet.hpMax, x0, y, w, hpColor(pet.hp, pet.hpMax))
            ui.color = dim
            y = text(canvas, pet.growth, x0, y, w, ui)
            pet.whereabouts?.let { ui.color = accent; y = text(canvas, it, x0, y, w, ui) }
        }

        // --- level overview
        y += dp(8f)
        y = header(canvas, "MAP", x0, y, w)
        val mapH = w * s.mapH / s.mapW
        mapBox.set(x0, y, x0 + w, y + mapH)
        renderer.drawOverview(canvas, s, mapBox)
        mapShown = true
        y += mapH + dp(6f)
        y += renderer.drawLegend(canvas, x0, y, w, dp(10f))

        // --- inventory
        y += dp(8f)
        y = header(canvas, "INVENTORY", x0, y, w)
        ui.textSize = dp(13f)
        var any = false
        for (i in 0 until minOf(s.invSlots, 26)) {
            val t = s.invText(i)
            if (t.isEmpty()) continue
            any = true
            y = item(canvas, "${'a' + i})", t, x0, y, w)
        }
        if (!any) { ui.color = dim; y = text(canvas, "Nothing", x0, y, w, ui) }

        canvas.restore()
        contentH = y + pad
        if (contentH > height) {   // scroll hint
            val track = height.toFloat()
            val thumb = track * (height / contentH)
            val ty = (track - thumb) * (scroll / (contentH - height))
            fill.color = Color.argb(70, 255, 255, 255)
            canvas.drawRoundRect(width - dp(4f), ty, width - dp(1.5f), ty + thumb, dp(2f), dp(2f), fill)
        }
    }

    private fun rowH() = dp(19f)

    companion object {
        /** Perk names by bit index; mirrors perk_name[] in compat/wild_perks.c
         *  (bit i of [GameState.perkBits] = perk i). Bits 10..15 aren't perks. */
        private val PERK_NAMES = listOf(
            "Magnet Hands", "Thick Skin", "Vampiric", "Bookworm", "Lucky",
            "Nimble", "Haggler", "Glass Cannon", "Second Wind", "Night Owl",
        )
    }

    private fun header(canvas: Canvas, label: String, x: Float, y: Float, w: Float): Float {
        bold.textSize = dp(11f); bold.color = dim
        canvas.drawText(label, x, y + dp(14f) - dp(3f), bold)
        fill.color = line
        canvas.drawRect(x, y + dp(16f), x + w, y + dp(17f), fill)
        return y + dp(22f)
    }

    /** One line of text, shrunk to fit [w] if needed. */
    private fun text(canvas: Canvas, t: String, x: Float, y: Float, w: Float, p: Paint): Float {
        val base = p.textSize
        val tw = p.measureText(t)
        if (tw > w) p.textSize = maxOf(dp(9f), base * w / tw)
        canvas.drawText(t, x, y + rowH() / 2 - (p.descent() + p.ascent()) / 2, p)
        p.textSize = base
        return y + rowH()
    }

    private fun drawLabelValue(canvas: Canvas, label: String, value: String, x: Float, y: Float, w: Float) {
        val base = y + rowH() / 2 - (ui.descent() + ui.ascent()) / 2
        ui.color = dim
        canvas.drawText(label, x, base, ui)
        ui.color = fg
        canvas.drawText(value, x + w - ui.measureText(value), base, ui)
    }

    private fun bar(canvas: Canvas, label: String, v: Int, max: Int, x: Float, y: Float, w: Float, color: Int): Float {
        ui.color = fg
        val txt = "$label $v/$max"
        val base = y + rowH() / 2 - (ui.descent() + ui.ascent()) / 2
        canvas.drawText(txt, x, base, ui)
        val l = x + ui.measureText("Spells 999/999") + dp(8f)
        val r = x + w
        if (r - l > dp(24f)) {
            val cy = y + rowH() / 2
            fill.color = line
            canvas.drawRoundRect(l, cy - dp(4f), r, cy + dp(4f), dp(4f), dp(4f), fill)
            val f = if (max > 0) (v.toFloat() / max).coerceIn(0f, 1f) else 0f
            fill.color = color
            if (f > 0f) canvas.drawRoundRect(l, cy - dp(4f), l + (r - l) * f, cy + dp(4f), dp(4f), dp(4f), fill)
        }
        return y + rowH()
    }

    private fun hpColor(hp: Int, max: Int): Int {
        val f = if (max > 0) hp.toFloat() / max else 0f
        return when {
            f > 0.6f -> Color.rgb(90, 200, 110)
            f > 0.3f -> Color.rgb(240, 190, 60)
            else -> Color.rgb(235, 80, 70)
        }
    }

    /** An inventory line: "a)" then the text, word-wrapped under itself. */
    private fun item(canvas: Canvas, letter: String, t: String, x: Float, y: Float, w: Float): Float {
        ui.color = accent
        val indent = ui.measureText("m) ") + dp(2f)
        canvas.drawText(letter, x, y + rowH() / 2 - (ui.descent() + ui.ascent()) / 2, ui)
        ui.color = fg
        var yy = y
        val avail = w - indent
        var rest = t.trim()
        while (rest.isNotEmpty()) {
            var n = ui.breakText(rest, true, avail, null)
            if (n <= 0) n = 1
            if (n < rest.length) {
                val sp = rest.lastIndexOf(' ', n)
                if (sp > 0) n = sp
            }
            canvas.drawText(rest, 0, n, x + indent, yy + rowH() / 2 - (ui.descent() + ui.ascent()) / 2, ui)
            yy += rowH()
            rest = rest.substring(n).trimStart()
        }
        return yy
    }

    // ------------------------------------------------------------ touch

    private val touchSlop = ViewConfiguration.get(context).scaledTouchSlop
    private var downY = 0f
    private var lastY = 0f
    private var dragging = false

    @SuppressLint("ClickableViewAccessibility")
    override fun onTouchEvent(e: MotionEvent): Boolean {
        when (e.actionMasked) {
            MotionEvent.ACTION_DOWN -> { downY = e.y; lastY = e.y; dragging = false }
            MotionEvent.ACTION_MOVE -> {
                if (!dragging && abs(e.y - downY) > touchSlop) dragging = true
                if (dragging) {
                    scroll = (scroll - (e.y - lastY)).coerceIn(0f, maxOf(0f, contentH - height))
                    invalidate()
                }
                lastY = e.y
            }
            MotionEvent.ACTION_UP -> {
                if (!dragging && mapShown && mapBox.contains(e.x, e.y + scroll)) {
                    LevelOverviewDialog.show(context, stateOf)
                }
                dragging = false
            }
            MotionEvent.ACTION_CANCEL -> dragging = false
        }
        return true
    }
}
