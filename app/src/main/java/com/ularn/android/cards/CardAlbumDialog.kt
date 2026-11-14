package com.ularn.android.cards

import android.app.AlertDialog
import android.app.Dialog
import android.content.Context
import android.graphics.Color
import android.view.Gravity
import android.view.View
import android.view.ViewGroup
import android.widget.AdapterView
import android.widget.ArrayAdapter
import android.widget.BaseAdapter
import android.widget.GridView
import android.widget.LinearLayout
import android.widget.Spinner
import android.widget.TextView
import java.text.DateFormat
import java.util.Date

/**
 * The card album: every card in monster order, owned ones in full and the
 * rest as "???" silhouettes, with a progress count and two filters: by
 * rarity, and by whether you have the card (all / found / holo / not yet).
 * Tapping a card opens it full screen (holo cards shimmer).
 */
object CardAlbumDialog {

    fun show(context: Context, album: CardAlbum, renderer: CardRenderer) {
        val d = context.resources.displayMetrics.density
        fun dp(v: Int) = (v * d).toInt()
        val thumbDp = 92
        val thumbScale = renderer.scaleFor(dp(thumbDp))

        val progress = TextView(context).apply { textSize = 15f; setPadding(0, 0, 0, dp(4)) }
        val filters = listOf<Pair<String, Rarity?>>("All rarities" to null) + Rarity.entries.map { it.label to it }
        val spinner = Spinner(context).apply {
            adapter = ArrayAdapter(context, android.R.layout.simple_spinner_dropdown_item, filters.map { it.first })
            contentDescription = "Filter by rarity"
        }
        val ownedSpinner = Spinner(context).apply {
            adapter = ArrayAdapter(context, android.R.layout.simple_spinner_dropdown_item, Owned.entries.map { it.label })
            contentDescription = "Filter by found or not found"
        }

        var ids = album.ids(null)
        val grid = GridView(context).apply {
            numColumns = GridView.AUTO_FIT
            columnWidth = dp(thumbDp)
            stretchMode = GridView.STRETCH_SPACING_UNIFORM
            verticalSpacing = dp(10)
            horizontalSpacing = dp(6)
            clipToPadding = false
            setPadding(0, dp(6), 0, dp(6))
        }
        val adapter = object : BaseAdapter() {
            override fun getCount() = ids.size
            override fun getItem(position: Int) = ids[position]
            override fun getItemId(position: Int) = ids[position].toLong()
            override fun getView(position: Int, convertView: View?, parent: ViewGroup): View {
                val cell = (convertView as? LinearLayout) ?: LinearLayout(context).apply {
                    orientation = LinearLayout.VERTICAL
                    gravity = Gravity.CENTER_HORIZONTAL
                    addView(CardView(context), LinearLayout.LayoutParams(dp(thumbDp), dp(thumbDp) * CardRenderer.H / CardRenderer.W))
                    addView(TextView(context).apply { textSize = 12f; gravity = Gravity.CENTER })
                }
                val id = ids[position]
                val e = album[id]
                (cell.getChildAt(0) as CardView).setCard(renderer.render(id, e, thumbScale), e?.holo == true)
                (cell.getChildAt(1) as TextView).text = caption(e)
                cell.contentDescription = describe(id, e)
                return cell
            }
        }
        grid.adapter = adapter
        grid.setOnItemClickListener { _, _, pos, _ -> showCard(context, album, renderer, ids[pos]) }

        fun refresh() {
            val r = filters[spinner.selectedItemPosition.coerceAtLeast(0)].second
            val o = Owned.entries[ownedSpinner.selectedItemPosition.coerceAtLeast(0)]
            ids = album.ids(r, o)
            progress.text = album.summary(r, o)
            adapter.notifyDataSetChanged()
        }
        val onPick = object : AdapterView.OnItemSelectedListener {
            override fun onItemSelected(p: AdapterView<*>?, v: View?, pos: Int, id: Long) = refresh()
            override fun onNothingSelected(p: AdapterView<*>?) {}
        }
        spinner.onItemSelectedListener = onPick
        ownedSpinner.onItemSelectedListener = onPick

        val box = LinearLayout(context).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(dp(16), dp(8), dp(16), 0)
            addView(progress)
            addView(LinearLayout(context).apply {
                orientation = LinearLayout.HORIZONTAL
                addView(spinner, LinearLayout.LayoutParams(0, -2, 1f))
                addView(ownedSpinner, LinearLayout.LayoutParams(0, -2, 1f))
            })
            if (album.complete) addView(TextView(context).apply {
                text = "Complete set! Gotta kill 'em all - and you did."
                setTextColor(Color.rgb(255, 200, 80))
            })
            val h = (context.resources.displayMetrics.heightPixels * 0.62f).toInt()
            addView(grid, LinearLayout.LayoutParams(-1, h))
        }
        refresh()
        AlertDialog.Builder(context)
            .setTitle("Card album")
            .setView(box)
            .setPositiveButton("Close", null)
            .show()
    }

    /** Full-screen view of one card; tap anywhere to close. */
    fun showCard(context: Context, album: CardAlbum, renderer: CardRenderer, id: Int) {
        val d = context.resources.displayMetrics.density
        val dm = context.resources.displayMetrics
        val e = album[id]
        val dialog = Dialog(context, android.R.style.Theme_Black_NoTitleBar_Fullscreen)
        val maxW = minOf((dm.widthPixels * 0.86f).toInt(), (dm.heightPixels * 0.72f * CardRenderer.W / CardRenderer.H).toInt())
        val card = CardView(context).apply {
            setCard(renderer.render(id, e, renderer.scaleFor(maxW)), e?.holo == true)
            contentDescription = describe(id, e)
        }
        val info = TextView(context).apply {
            setTextColor(Color.LTGRAY)
            gravity = Gravity.CENTER
            textSize = 14f
            setPadding(0, (14 * d).toInt(), 0, 0)
            text = buildString {
                if (e == null) append("Not found yet")
                else {
                    append("Owned: ${e.count}")
                    if (e.holoCount > 0) append(" (${e.holoCount} holo)")
                    if (e.firstSeenAt > 0) append("\nFirst found ").append(DateFormat.getDateInstance().format(Date(e.firstSeenAt)))
                }
                append("\n\nTap to close")
            }
        }
        val root = LinearLayout(context).apply {
            orientation = LinearLayout.VERTICAL
            gravity = Gravity.CENTER
            setBackgroundColor(Color.argb(240, 8, 8, 10))
            addView(card, LinearLayout.LayoutParams(maxW, maxW * CardRenderer.H / CardRenderer.W))
            addView(info)
            setOnClickListener { dialog.dismiss() }
        }
        dialog.setContentView(root)
        dialog.show()
    }

    private fun caption(e: CardEntry?): String = when {
        e == null -> "???"
        e.holoCount > 0 -> "★ x${e.count}"
        else -> "x${e.count}"
    }

    private fun describe(id: Int, e: CardEntry?): String {
        val f = MonsterFacts.get(id) ?: return "Unknown card"
        if (e == null) return "Card ${id}: not found yet"
        return "${f.title}, ${f.rarity.label}${if (e.holo) ", holo" else ""}, owned ${e.count}. " +
            "Level ${f.level}, armour ${f.ac}, damage ${f.dam}, hit points ${f.hp}, experience ${f.exp}. ${f.lore}"
    }
}
