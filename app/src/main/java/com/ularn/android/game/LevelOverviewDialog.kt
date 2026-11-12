package com.ularn.android.game

import android.annotation.SuppressLint
import android.app.Dialog
import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.RectF
import android.graphics.Typeface
import android.view.MotionEvent
import android.view.View
import android.view.WindowManager
import com.ularn.android.Settings

/**
 * The whole current level, fitted to the screen in the minimap's dot style,
 * with a legend. Updates live; a tap or Back closes it.
 */
object LevelOverviewDialog {

    fun show(context: Context, stateOf: () -> GameState?) {
        val dialog = Dialog(context, android.R.style.Theme_Black_NoTitleBar_Fullscreen)
        dialog.setContentView(OverviewView(context, stateOf) { dialog.dismiss() })
        dialog.setCanceledOnTouchOutside(true)
        if (Settings(context).keepScreenOn) dialog.window?.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        dialog.show()
    }

    /** Draws a level overview, title and legend into a view. */
    private class OverviewView(
        context: Context,
        private val stateOf: () -> GameState?,
        private val onClose: () -> Unit,
    ) : View(context) {

        private val density = resources.displayMetrics.density
        private fun dp(v: Float) = v * density
        private val renderer = MinimapRenderer(density)
        private val title = Paint(Paint.ANTI_ALIAS_FLAG).apply {
            typeface = Typeface.create(Typeface.SANS_SERIF, Typeface.BOLD); color = Color.WHITE
        }
        private val hint = Paint(Paint.ANTI_ALIAS_FLAG).apply { typeface = Typeface.SANS_SERIF; color = Color.rgb(150, 150, 150) }
        private val area = RectF()

        private var lastSeq = Int.MIN_VALUE
        private val poll = object : Runnable {
            override fun run() {
                val seq = stateOf()?.liveSeq()
                if (seq != null && seq != lastSeq) invalidate()
                postDelayed(this, POLL_MS)
            }
        }

        override fun onAttachedToWindow() { super.onAttachedToWindow(); postDelayed(poll, POLL_MS) }
        override fun onDetachedFromWindow() { removeCallbacks(poll); super.onDetachedFromWindow() }

        override fun onDraw(canvas: Canvas) {
            canvas.drawColor(Color.rgb(12, 12, 14))
            val pad = dp(16f)
            title.textSize = dp(18f)
            hint.textSize = dp(12f)
            var y = pad - title.ascent()
            val s = stateOf()
            if (s == null || !s.refresh() || s.seq == 0) {
                canvas.drawText("No map yet", pad, y, title)
                return
            }
            lastSeq = s.seq
            val where = s.levelName.let { if (it == "H") "Home" else if (it.isEmpty()) "" else "Level $it" }
            canvas.drawText(if (where.isEmpty()) "Level map" else "$where — map", pad, y, title)
            val h = "Tap or Back to close"
            canvas.drawText(h, width - pad - hint.measureText(h), y, hint)
            y += title.descent() + dp(12f)

            val legendText = dp(13f)
            val legendW = width - 2 * pad
            val legendH = renderer.drawLegend(null, pad, 0f, legendW, legendText)
            area.set(pad, y, width - pad, height - pad - legendH - dp(12f))
            if (area.height() > dp(20f)) {
                val map = renderer.drawOverview(canvas, s, area)
                renderer.drawLegend(canvas, pad, map.bottom + dp(12f), legendW, legendText)
            }
        }

        @SuppressLint("ClickableViewAccessibility")
        override fun onTouchEvent(e: MotionEvent): Boolean {
            if (e.actionMasked == MotionEvent.ACTION_UP) onClose()
            return true
        }

        companion object {
            private const val POLL_MS = 250L
        }
    }
}
