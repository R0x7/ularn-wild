package com.ularn.android

import android.app.Activity
import android.app.AlertDialog
import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.content.res.Configuration
import android.graphics.Color
import android.graphics.Typeface
import android.graphics.drawable.GradientDrawable
import android.os.Build
import android.os.Bundle
import android.view.Gravity
import android.view.KeyEvent
import android.view.View
import android.view.WindowInsets
import android.view.WindowInsetsController
import android.view.WindowManager
import android.widget.Button
import android.widget.CheckBox
import android.widget.EditText
import android.widget.FrameLayout
import android.widget.HorizontalScrollView
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.SeekBar
import android.widget.Spinner
import android.widget.ArrayAdapter
import android.widget.TextView
import android.widget.Toast
import com.ularn.android.controls.ButtonAction
import com.ularn.android.controls.ControlEditorDialog
import com.ularn.android.controls.ControlLayout
import com.ularn.android.controls.ControlSpec
import com.ularn.android.controls.ControlType
import com.ularn.android.controls.ControlsOverlay
import com.ularn.android.controls.LayoutStore
import com.ularn.android.game.ContextAction
import com.ularn.android.game.GameEvents
import com.ularn.android.game.GameView
import com.ularn.android.game.SidePanelController
import com.ularn.android.game.KeyFollowUp
import com.ularn.android.stats.Achievement
import com.ularn.android.stats.RunTracker
import com.ularn.android.stats.StatsDialog
import com.ularn.android.a11y.AccessibilityPrefs
import com.ularn.android.cards.CardDrops
import com.ularn.android.cards.MonsterFacts
import com.ularn.android.a11y.AccessibilitySettingsUi
import com.ularn.android.a11y.MessageAnnouncer
import com.ularn.android.audio.Radio
import com.ularn.android.saves.SlotsController
import com.ularn.android.photo.PhotoMode

class MainActivity : Activity(), ControlsOverlay.Listener {

    private lateinit var settings: Settings
    private lateinit var store: LayoutStore
    private lateinit var session: GameSession
    private lateinit var contextAction: ContextAction
    private lateinit var spellPicker: SpellPicker
    private lateinit var tracker: RunTracker
    /** Wild Edition event bus; features register listeners here. */
    lateinit var events: GameEvents
    private lateinit var slotsUi: SlotsController
    private lateinit var a11yPrefs: AccessibilityPrefs
    private lateinit var announcer: MessageAnnouncer
    private lateinit var cards: CardDrops
    /** Wild Edition: Dungeon Radio (music + sound effects). */
    private lateinit var radio: Radio
    /** Wild Edition: Polaroid photo mode. */
    private lateinit var photos: PhotoMode

    private lateinit var game: GameView
    private lateinit var overlay: ControlsOverlay
    private lateinit var editBar: LinearLayout
    private lateinit var editBarScroll: HorizontalScrollView
    private lateinit var startPanel: LinearLayout
    private lateinit var startScroll: ScrollView
    private lateinit var startTitle: TextView

    private var landscape = false
    private var keyboardShown = false
    private var quitAfterExit = false
    private var editBarAtTop = true
    /** Tablet / foldable side panel (null until onCreate builds it). */
    @Volatile private var sidePanel: SidePanelController? = null
    /** Layout snapshot taken when entering edit mode (for Cancel). */
    private var editBackup: ControlLayout? = null

    private val density get() = resources.displayMetrics.density
    private fun dp(v: Int) = (v * density).toInt()

    // ------------------------------------------------------------ lifecycle

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        if (Build.VERSION.SDK_INT >= 28) {
            window.attributes = window.attributes.apply {
                layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES
            }
        }

        settings = Settings(this)
        store = LayoutStore(this)
        tracker = RunTracker(this).apply { onUnlocked = { showUnlocked(it) } }
        a11yPrefs = AccessibilityPrefs(this)

        game = GameView(this).apply {
            onInput = { if (session.isRunning) session.send(it) }
            onIdleTap = { if (!session.isRunning) showStartPanel(true) }
            onToggleKeyboard = { setKeyboard(!keyboardShown) }
            onZoomChanged = { settings.tileDp = it }
        }
        overlay = ControlsOverlay(this).apply { listener = this@MainActivity }
        // Recompute the map area after the new size is known (rotation,
        // split screen, foldables) - not before, as onConfigurationChanged is.
        overlay.addOnLayoutChangeListener { _, l, t, r, b, ol, ot, or_, ob ->
            if (r - l != or_ - ol || b - t != ob - ot) { ensureLayoutLoaded(); updateMapReserve() }
        }

        session = GameSession(
            this,
            onChanged = { game.postInvalidateOnAnimation(); sidePanel?.changed(); contextAction.onGameChanged(); announcer.onStateChanged(); events.poll() },
            onExit = { onGameExit() },
        )
        game.state = session.state
        events = GameEvents { session.state }   // Wild Edition: C-side events (sound, cards...)
        cards = CardDrops(this, settings) { session.state?.runId ?: 0 }.also { events.addListener(it) }
        radio = Radio(this, settings, { session.state }, { session.isRunning }).also { events.addListener(it) }
        photos = PhotoMode(this, settings, game, { session.state },
            characterName = { com.ularn.android.photo.PolaroidMath.nameFromOpts(session.optsText) },
            radioShutter = {
                // The Radio's "take" blip doubles as the shutter click when it's on and audible.
                if (settings.wild(Settings.WILDF_RADIO) && settings.radioSfxVolume > 0) {
                    radio.onEvent(GameEvents.Event(GameEvents.TAKE, 0, 0, 0)); true
                } else false
            }).also { events.addListener(it) }
        // Wild Edition weather: a lightning strike from the game flashes the screen
        events.addListener { e -> if (e.type == GameEvents.WILD_WEATHER && e.c == 1) game.weather.flash() }
        val followUp = KeyFollowUp({ session.state }) { if (session.isRunning) session.send(it) }
        contextAction = ContextAction({ session.state }, { session.isRunning }, followUp) { overlay.contextUi = it }
        spellPicker = SpellPicker(this, { session.state }, { session.isRunning }, followUp) { if (session.isRunning) session.send(it) }
        game.onFrame = { if (session.isRunning) tracker.observe(it) }
        game.contentDescription = "Game screen"
        announcer = MessageAnnouncer(game) { session.state }
        slotsUi = SlotsController(this, session, settings, startGame = { startGame(it) },
            refresh = { if (startScroll.visibility == View.VISIBLE) showStartPanel(true) })
        slotsUi.migrate()

        val root = FrameLayout(this).apply { setBackgroundColor(Color.BLACK) }
        root.addView(game, FrameLayout.LayoutParams(-1, -1))
        root.addView(overlay, FrameLayout.LayoutParams(-1, -1))
        buildEditBar(root)
        buildStartPanel(root)
        sidePanel = SidePanelController(root, listOf(game, overlay)) { session.state }
        setContentView(root)

        root.setOnApplyWindowInsetsListener { v, insets ->
            if (Build.VERSION.SDK_INT >= 28) {
                val cut = insets.displayCutout
                v.setPadding(cut?.safeInsetLeft ?: 0, cut?.safeInsetTop ?: 0, cut?.safeInsetRight ?: 0, cut?.safeInsetBottom ?: 0)
            }
            if (Build.VERSION.SDK_INT >= 30) {
                val ime = insets.getInsets(WindowInsets.Type.ime()).bottom
                setImeHeight(if (insets.isVisible(WindowInsets.Type.ime())) maxOf(0, ime - v.paddingBottom) else 0)
            }
            insets
        }
        // Before Android 11 there are no IME insets: measure how much of the
        // game the visible window frame leaves out instead.
        if (Build.VERSION.SDK_INT < 30) root.viewTreeObserver.addOnGlobalLayoutListener {
            if (!keyboardShown) { setImeHeight(0); return@addOnGlobalLayoutListener }
            val frame = android.graphics.Rect()
            root.getWindowVisibleDisplayFrame(frame)
            val loc = IntArray(2)
            game.getLocationOnScreen(loc)
            setImeHeight(maxOf(0, loc[1] + game.height - frame.bottom))
        }

        applySettings()
        applyRotationSetting()
        applyOrientation(resources.configuration)
        showStartPanel(true)
    }

    override fun onWindowFocusChanged(hasFocus: Boolean) {
        super.onWindowFocusChanged(hasFocus)
        if (hasFocus) hideSystemBars()
    }

    override fun onConfigurationChanged(newConfig: Configuration) {
        super.onConfigurationChanged(newConfig)
        val nowLandscape = newConfig.orientation == Configuration.ORIENTATION_LANDSCAPE
        if (nowLandscape != landscape) {
            if (overlay.editMode) finishEditing(save = true)
            setKeyboard(false)
            applyOrientation(newConfig)
        }
        applyA11y()   // font scale
    }

    override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<out String>, grantResults: IntArray) {
        if (!photos.onRequestPermissionsResult(requestCode, grantResults))
            super.onRequestPermissionsResult(requestCode, permissions, grantResults)
    }

    @Deprecated("Deprecated in Java")
    @Suppress("DEPRECATION")
    override fun onActivityResult(requestCode: Int, resultCode: Int, data: android.content.Intent?) {
        if (!slotsUi.onActivityResult(requestCode, resultCode, data)) super.onActivityResult(requestCode, resultCode, data)
    }

    private fun applyA11y() {
        if (!::overlay.isInitialized || !::announcer.isInitialized) return
        val scale = a11yPrefs.effectiveScale(resources.configuration)
        game.textScale = scale; game.highContrast = a11yPrefs.highContrast
        overlay.textScale = scale; overlay.highContrast = a11yPrefs.highContrast
        announcer.enabled = a11yPrefs.announceMessages
    }

    override fun onResume() {
        super.onResume()
        radio.onResume()
    }

    override fun onPause() {
        radio.onPause()   // no music while the app is in the background
        super.onPause()
    }

    override fun onStop() {
        // Android may kill a backgrounded app without warning, so make sure a
        // fresh checkpoint exists ("Resume from checkpoint" next time).
        if (session.isRunning) { session.requestCheckpoint(); slotsUi.recordProgress() }
        tracker.flush()
        super.onStop()
    }

    override fun onDestroy() {
        if (isFinishing) session.leave()
        super.onDestroy()
    }

    @Deprecated("Deprecated in Java")
    @Suppress("DEPRECATION")
    override fun onBackPressed() = handleBack()

    override fun onKeyDown(keyCode: Int, event: KeyEvent): Boolean {
        if (keyCode == KeyEvent.KEYCODE_BACK || keyCode == KeyEvent.KEYCODE_MENU) {
            if (keyCode == KeyEvent.KEYCODE_MENU) { showMenu(); return true }
            return super.onKeyDown(keyCode, event)
        }
        if (session.isRunning && !overlay.editMode && game.handleKey(keyCode, event)) return true
        return super.onKeyDown(keyCode, event)
    }

    private fun handleBack() {
        when {
            overlay.editMode -> finishEditing(save = true)
            keyboardShown -> setKeyboard(false)
            else -> showMenu()
        }
    }

    private fun hideSystemBars() {
        if (Build.VERSION.SDK_INT >= 30) {
            window.insetsController?.let {
                it.hide(WindowInsets.Type.systemBars())
                it.systemBarsBehavior = WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
            }
        } else {
            @Suppress("DEPRECATION")
            window.decorView.systemUiVisibility = (View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                or View.SYSTEM_UI_FLAG_FULLSCREEN or View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                or View.SYSTEM_UI_FLAG_LAYOUT_STABLE or View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                or View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN)
        }
    }

    /** Which orientation the overlay's current layout was loaded for. */
    private var layoutLoadedFor: Boolean? = null

    private fun applyOrientation(cfg: Configuration) {
        landscape = cfg.orientation == Configuration.ORIENTATION_LANDSCAPE
        layoutLoadedFor = null
        ensureLayoutLoaded()
        updateMapReserve()
    }

    private val overlayWidthDp get() = overlay.width / density
    private val overlayHeightDp get() = overlay.height / density

    /** Loads the layout for the current orientation once the overlay has a size. */
    private fun ensureLayoutLoaded() {
        if (layoutLoadedFor == landscape || overlay.width == 0 || overlay.height == 0) return
        if (overlay.editMode) return
        overlay.layout = store.load(landscape, overlayWidthDp, overlayHeightDp)
        game.haptics = overlay.layout.haptics
        layoutLoadedFor = landscape
    }

    /**
     * Keep the game's display clear of the controls: in landscape measure the
     * controls hugging the left and right edges; in portrait, the block of
     * controls along the bottom. Capped so the map always keeps some room.
     */
    private fun updateMapReserve() {
        overlay.post {
            val w = overlay.width.toFloat()
            val h = overlay.height.toFloat()
            if (w == 0f || h == 0f) return@post
            val r = android.graphics.RectF()
            var left = 0f; var right = 0f; var bottomTop = h
            for (c in overlay.layout.controls) {
                overlay.rectFor(c, r)
                if (landscape) {
                    if (r.centerX() < w * 0.25f) left = maxOf(left, r.right)
                    else if (r.centerX() > w * 0.75f) right = maxOf(right, w - r.left)
                } else if (r.centerY() > h * 0.3f) {
                    bottomTop = minOf(bottomTop, r.top)
                }
            }
            game.minimap.setAvoid(if (landscape) minOf(left, w * 0.3f) else 0f, if (landscape) minOf(right, w * 0.3f) else 0f)
            if (landscape) {
                if (!settings.fitMapBesideControls) { left = 0f; right = 0f }
                game.setReserve(minOf(left, w * 0.3f), minOf(right, w * 0.3f), imeHeight.toFloat())
            } else {
                game.setReserve(0f, 0f, maxOf(minOf(h - bottomTop + dp(4), h * 0.65f), imeHeight.toFloat()))
            }
        }
    }

    private fun applySettings() {
        game.arrowsAsMoves = settings.arrowsAsMoves
        game.tapToMove = settings.tapToMove
        game.swipeToMove = settings.swipeToMove
        game.flickToRun = settings.flickToRun
        game.tileDp = settings.tileDp
        game.minimap.enabled = settings.showMinimap
        sidePanel?.enabled = settings.sidePanel
        if (::overlay.isInitialized) game.haptics = overlay.layout.haptics
        if (::overlay.isInitialized) updateMapReserve()
        game.danger.apply(settings)
        game.glitch.reduce = settings.reduceGlitch
        game.weather.reduce = settings.reduceWeather
        applyKeepScreenOn()
        applyA11y()
    }

    private fun applyKeepScreenOn() {
        val flag = WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON
        if (settings.keepScreenOn) window.addFlags(flag) else window.clearFlags(flag)
    }

    private fun applyRotationSetting() {
        requestedOrientation = Settings.ORIENTATIONS
            .getOrElse(settings.orientation) { Settings.ORIENTATIONS[0] }.second
    }

    /** Menu shortcut: lock to the other orientation. */
    private fun toggleOrientation() {
        settings.orientation = if (landscape) Settings.ORIENT_PORTRAIT else Settings.ORIENT_LANDSCAPE
        applyRotationSetting()
    }

    // --------------------------------------------------------------- game

    private fun startGame(args: List<String> = emptyList()) {
        showStartPanel(false)
        quitAfterExit = false
        announcer.reset()
        if (!session.start(settings, args)) {
            toast("Couldn't start the game")
            showStartPanel(true)
            return
        }
        game.running = true
        game.requestFocus()
    }

    private fun onGameExit() {
        tracker.onGameExit(session.state, session.hasSave() || session.hasCheckpoint())
        slotsUi.recordProgress()
        game.running = false
        setKeyboard(false)
        if (quitAfterExit) { finish(); return }
        // The game has already shown its final pages (score, scoreboard) and
        // waited for "return" on each, so go straight back to the main menu.
        if (!overlay.editMode) showStartPanel(true)
    }

    // ---------------------------------------------- ControlsOverlay.Listener

    override fun onKeys(bytes: ByteArray) {
        if (session.isRunning) session.send(bytes)
    }

    override fun onAction(action: ButtonAction) {
        when (action) {
            ButtonAction.KEYBOARD -> setKeyboard(!keyboardShown)
            ButtonAction.MENU -> showMenu()
            ButtonAction.EDIT -> startEditing()
            ButtonAction.CONTEXT -> contextAction.trigger()
            ButtonAction.CAST -> spellPicker.onCastButton()
            ButtonAction.PHOTO -> if (session.isRunning) photos.takePhoto()
            else -> {}
        }
    }

    override fun onEditControl(spec: ControlSpec) = openControlEditor(spec)

    override fun onLayoutEdited() {}

    override fun onSelectionChanged(spec: ControlSpec?) = refreshEditBar()

    /** Height of the on-screen keyboard over the game, in px (0 when hidden). */
    private var imeHeight = 0

    /** Keep the game's display, and so its prompt line, above the keyboard. */
    private fun setImeHeight(px: Int) {
        if (px == imeHeight) return
        imeHeight = px
        if (::overlay.isInitialized) updateMapReserve()
    }

    private fun setKeyboard(show: Boolean) {
        keyboardShown = show
        game.showKeyboard(show)
    }

    // ------------------------------------------------------------ editing

    private fun startEditing() {
        showStartPanel(false)
        setKeyboard(false)
        editBackup = overlay.layout.deepCopy()
        overlay.editMode = true
        editBarScroll.visibility = View.VISIBLE
        refreshEditBar()
        Toast.makeText(this, "Drag to move · corner handle to resize · tap a selected control to edit it", Toast.LENGTH_LONG).show()
    }

    private fun finishEditing(save: Boolean) {
        if (save) store.save(landscape, overlay.layout)
        else editBackup?.let { overlay.layout = it }
        editBackup = null
        overlay.editMode = false
        updateMapReserve()
        editBarScroll.visibility = View.GONE
        if (!session.isRunning) showStartPanel(true)
    }

    private fun pill(text: String, accent: Boolean = false, onClick: () -> Unit) =
        Button(this, null, android.R.attr.buttonStyleSmall).apply {
            this.text = text
            isAllCaps = false
            setTextColor(if (accent) Color.BLACK else Color.WHITE)
            background = GradientDrawable().apply {
                cornerRadius = dp(18).toFloat()
                setColor(if (accent) Color.rgb(255, 190, 60) else Color.argb(220, 45, 47, 55))
            }
            minHeight = dp(36); minimumHeight = dp(36)
            setPadding(dp(14), 0, dp(14), 0)
            setOnClickListener { onClick() }
        }

    private lateinit var editSelectedBtn: Button
    private lateinit var snapBtn: Button

    private fun buildEditBar(root: FrameLayout) {
        editBar = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            setPadding(dp(8), dp(6), dp(8), dp(6))
            gravity = Gravity.CENTER_VERTICAL
        }
        fun add(v: View) = editBar.addView(v, LinearLayout.LayoutParams(-2, -2).apply { marginEnd = dp(6) })

        add(pill("Done", accent = true) { finishEditing(save = true) })
        add(pill("+ Button") { addControl(ControlType.BUTTON) })
        add(pill("+ D-pad") { addControl(ControlType.DPAD) })
        editSelectedBtn = pill("Edit selected") { overlay.selected?.let { openControlEditor(it) } }
        add(editSelectedBtn)
        snapBtn = pill("Snap: on") {
            overlay.snapToGrid = !overlay.snapToGrid; refreshEditBar()
        }
        add(snapBtn)
        add(pill("Look") { showLookDialog() })
        add(pill("Profiles") { showProfilesDialog() })
        add(pill("Reset") {
            AlertDialog.Builder(this)
                .setMessage("Reset the ${if (landscape) "landscape" else "portrait"} layout to the default?")
                .setPositiveButton("Reset") { _, _ -> overlay.layout = store.reset(landscape, overlayWidthDp, overlayHeightDp); refreshEditBar() }
                .setNegativeButton("Cancel", null).show()
        })
        add(pill("⇅") { editBarAtTop = !editBarAtTop; positionEditBar() }.apply { contentDescription = "Move toolbar to the other edge" })
        add(pill("Cancel") { finishEditing(save = false) })

        editBarScroll = HorizontalScrollView(this).apply {
            addView(editBar)
            setBackgroundColor(Color.argb(200, 10, 10, 12))
            visibility = View.GONE
            isHorizontalScrollBarEnabled = false
        }
        root.addView(editBarScroll, FrameLayout.LayoutParams(-1, -2, Gravity.TOP))
    }

    private fun positionEditBar() {
        (editBarScroll.layoutParams as FrameLayout.LayoutParams).gravity = if (editBarAtTop) Gravity.TOP else Gravity.BOTTOM
        editBarScroll.requestLayout()
    }

    private fun refreshEditBar() {
        editSelectedBtn.isEnabled = overlay.selected != null
        editSelectedBtn.alpha = if (overlay.selected != null) 1f else 0.4f
        snapBtn.text = if (overlay.snapToGrid) "Snap: on" else "Snap: off"
    }

    private fun addControl(type: ControlType) {
        val l = overlay.layout
        val spec = if (type == ControlType.DPAD)
            ControlSpec(l.newId("dpad"), ControlType.DPAD, cx = 0.5f, cy = 0.6f, widthDp = 160f, heightDp = 160f, repeat = true)
        else
            ControlSpec(l.newId("btn"), ControlType.BUTTON, "New", "", cx = 0.5f, cy = 0.6f, widthDp = 56f, heightDp = 46f)
        l.controls += spec
        overlay.select(spec.id)
        openControlEditor(spec)
    }

    private fun openControlEditor(spec: ControlSpec) {
        ControlEditorDialog.show(
            this, spec,
            onSave = { edited ->
                val list = overlay.layout.controls
                val i = list.indexOfFirst { it.id == spec.id }
                if (i >= 0) list[i] = edited
                overlay.invalidate(); refreshEditBar()
            },
            onDelete = {
                overlay.layout.controls.removeAll { it.id == spec.id }
                overlay.select(null)
            },
            onDuplicate = {
                val copy = spec.deepCopy().apply {
                    id = overlay.layout.newId(if (type == ControlType.DPAD) "dpad" else "btn")
                    cx = (cx + 0.06f).coerceAtMost(1f); cy = (cy + 0.04f).coerceAtMost(1f)
                }
                overlay.layout.controls += copy
                overlay.select(copy.id)
            },
        )
    }

    private fun showLookDialog() {
        val l = overlay.layout
        val box = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL; setPadding(dp(20), dp(8), dp(20), 0) }
        val lbl = TextView(this)
        fun refresh() { lbl.text = "Opacity: ${(l.opacity * 100).toInt()}%" }
        refresh()
        box.addView(lbl)
        box.addView(SeekBar(this).apply {
            max = 90; progress = ((l.opacity - 0.1f) * 100).toInt()
            setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
                override fun onProgressChanged(s: SeekBar?, p: Int, u: Boolean) { l.opacity = 0.1f + p / 100f; refresh(); this@MainActivity.overlay.invalidate() }
                override fun onStartTrackingTouch(s: SeekBar?) {}
                override fun onStopTrackingTouch(s: SeekBar?) {}
            })
        })
        box.addView(CheckBox(this).apply {
            text = "Vibrate on press"; isChecked = l.haptics
            setOnCheckedChangeListener { _, v -> l.haptics = v; game.haptics = v }
        })
        AlertDialog.Builder(this).setTitle("Controls appearance").setView(box).setPositiveButton("OK", null).show()
    }

    private fun showProfilesDialog() {
        val names = store.profileNames()
        // Label -> action, so a profile's name can never be mistaken for a command.
        val items = mutableListOf<Pair<String, () -> Unit>>("Save current layout as…" to { askProfileName() })
        names.forEach { n -> items += "Load: $n" to { loadEditedLayout(store.loadProfile(n), "Loaded") } }
        items += "Copy layout to clipboard (JSON)" to {
            clipboard.setPrimaryClip(ClipData.newPlainText("Ularn controls", LayoutStore.toJson(overlay.layout)))
            toast("Layout copied")
        }
        items += "Paste layout from clipboard" to {
            val txt = clipboard.primaryClip?.getItemAt(0)?.coerceToText(this)?.toString()
            val parsed = txt?.let { runCatching { LayoutStore.fromJson(it) }.getOrNull() }
            if (parsed == null) toast("Clipboard doesn't contain a layout") else loadEditedLayout(parsed, "Pasted")
        }
        if (names.isNotEmpty()) items += "Delete a profile…" to {
            AlertDialog.Builder(this).setTitle("Delete profile")
                .setItems(names.toTypedArray()) { _, i -> store.deleteProfile(names[i]); toast("Deleted") }
                .show()
        }
        AlertDialog.Builder(this)
            .setTitle("Layout profiles")
            .setItems(items.map { it.first }.toTypedArray()) { _, which -> items[which].second() }
            .show()
    }

    private val clipboard get() = getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager

    private fun askProfileName() {
        val e = EditText(this).apply { hint = "Profile name"; isSingleLine = true }
        AlertDialog.Builder(this).setTitle("Save profile").setView(e)
            .setPositiveButton("Save") { _, _ ->
                val n = e.text.toString().trim()
                if (n.isNotEmpty()) { store.saveProfile(n, overlay.layout); toast("Saved \"$n\"") }
            }.setNegativeButton("Cancel", null).show()
    }

    /** Puts a loaded or pasted layout into the editor; Done keeps it, Cancel drops it. */
    private fun loadEditedLayout(layout: ControlLayout?, verb: String) {
        layout ?: return
        overlay.layout = layout; refreshEditBar(); toast("$verb — tap Done to keep it")
    }

    // ------------------------------------------------------- start panel

    private fun buildStartPanel(root: FrameLayout) {
        startPanel = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            gravity = Gravity.CENTER_HORIZONTAL
            setPadding(dp(24), dp(20), dp(24), dp(20))
            background = GradientDrawable().apply {
                cornerRadius = dp(18).toFloat()
                setColor(Color.argb(235, 22, 22, 26))
                setStroke(dp(1), Color.argb(90, 255, 255, 255))
            }
            elevation = dp(8).toFloat()
        }
        startTitle = TextView(this).apply {
            text = "ULARN"
            contentDescription = "Ularn"
            if (Build.VERSION.SDK_INT >= 28) isAccessibilityHeading = true
            textSize = 30f
            typeface = Typeface.create(Typeface.MONOSPACE, Typeface.BOLD)
            setTextColor(Color.rgb(255, 190, 60))
            gravity = Gravity.CENTER
        }
        startScroll = ScrollView(this).apply {
            isVerticalScrollBarEnabled = false
            clipToPadding = false
            setPadding(0, dp(12), 0, dp(12))
            addView(startPanel)
        }
        root.addView(startScroll, FrameLayout.LayoutParams(dp(300), -2, Gravity.CENTER))
    }

    private fun showStartPanel(show: Boolean) {
        if (!show) { startScroll.visibility = View.GONE; return }
        startPanel.removeAllViews()
        startPanel.addView(startTitle)
        startPanel.addView(TextView(this).apply {
            text = "The Caverns of Ularn"
            setTextColor(Color.LTGRAY); gravity = Gravity.CENTER
            setPadding(0, 0, 0, dp(14))
        })
        fun item(label: String, accent: Boolean = false, action: () -> Unit) {
            startPanel.addView(pill(label, accent, action), LinearLayout.LayoutParams(-1, dp(44)).apply { topMargin = dp(8) })
        }
        // Save slots: continue / resume / new game / rename / delete / export / import.
        slotsUi.populate(startPanel) { label, accent, action -> item(label, accent, action) }
        item("Scoreboard") { slotsUi.useNoSlot(); startGame(listOf("-s")) }
        item("Statistics & achievements") { StatsDialog.show(this, tracker.book) }
        if (cards.enabled) item("Card album (${cards.album.collected}/${MonsterFacts.COUNT})") { cards.showAlbum() }
        item("Edit controls") { startEditing() }
        item("Settings") { showSettings() }
        item("Quit") { finish() }
        startScroll.visibility = View.VISIBLE
        startScroll.scrollTo(0, 0)
    }

    // --------------------------------------------------------------- menu

    private fun showMenu() {
        if (!session.isRunning) {
            if (startScroll.visibility != View.VISIBLE) showStartPanel(true) else finish()
            return
        }
        // Label -> action, so features can append entries without index juggling.
        val items = mutableListOf<Pair<String, () -> Unit>>(
            (if (keyboardShown) "Hide keyboard" else "Show keyboard") to { setKeyboard(!keyboardShown) },
            (if (landscape) "Switch to portrait" else "Switch to landscape") to { toggleOrientation() },
            "Edit controls" to { startEditing() },
            "Settings" to { showSettings() },
            "Game help (?)" to { session.send(byteArrayOf('?'.code.toByte())) },
            "Save game & quit" to { quitAfterExit = true; session.send(byteArrayOf('S'.code.toByte())) },
            "Leave (resume from checkpoint later)" to {
                AlertDialog.Builder(this)
                    .setMessage("Leave now? The game is checkpointed, so \"Resume from checkpoint\" picks up from here (or from your last few moves if you're in the middle of a menu).")
                    .setPositiveButton("Leave") { _, _ -> quitAfterExit = true; session.leave() }
                    .setNegativeButton("Cancel", null).show()
            },
            "Statistics & achievements" to { StatsDialog.show(this, tracker.book) },
        )
        // Wild Edition entries
        if (cards.enabled) items += "Card album" to { cards.showAlbum() }
        if (settings.wild(Settings.WILDF_RADIO)) items += "Radio" to { radio.showDialog() }
        if (photos.enabled) items += "Take a photo" to { photos.takePhoto() }
        AlertDialog.Builder(this).setItems(items.map { it.first }.toTypedArray()) { _, which ->
            items[which].second()
        }.show()
    }

    // ----------------------------------------------------------- settings

    private fun showSettings() {
        val box = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL; setPadding(dp(20), dp(8), dp(20), 0) }
        fun header(t: String) = box.addView(TextView(this).apply {
            text = t; setTypeface(typeface, Typeface.BOLD); setPadding(0, dp(14), 0, dp(2))
        })
        fun check(t: String, v: Boolean, set: (Boolean) -> Unit) = box.addView(CheckBox(this).apply {
            text = t; isChecked = v; setOnCheckedChangeListener { _, b -> set(b) }
        })

        header("Character (default for new games)")
        val name = EditText(this).apply { setText(settings.playerName); isSingleLine = true; hint = "Name" }
        box.addView(name)
        check("Female character", settings.female) { settings.female = it }
        check("Skip introduction", settings.skipIntro) { settings.skipIntro = it }
        check("No delays (no-nap)", settings.noNap) { settings.noNap = it }
        check("No beeping", settings.noBeep) { settings.noBeep = it }
        check("Enhanced interface: open doors with 'o', chests with 'O'", settings.enhancedInterface) { settings.enhancedInterface = it }

        header("Screen")
        val rotation = Spinner(this).apply {
            adapter = ArrayAdapter(this@MainActivity, android.R.layout.simple_spinner_dropdown_item, Settings.ORIENTATIONS.map { it.first })
            setSelection(settings.orientation.coerceIn(0, Settings.ORIENTATIONS.size - 1))
        }
        box.addView(rotation)
        box.addView(TextView(this).apply {
            text = "Portrait and landscape each keep their own control layout."; textSize = 12f; alpha = 0.7f
        })
        check("Keep the screen on while the app is open", settings.keepScreenOn) { settings.keepScreenOn = it }

        header("Map")
        check("Swipe to step, hold to keep walking", settings.swipeToMove) { settings.swipeToMove = it }
        check("Quick flick runs (instead of one step)", settings.flickToRun) { settings.flickToRun = it }
        check("Tap a square to step towards it (long-press to run)", settings.tapToMove) { settings.tapToMove = it }
        check("Landscape: fit the map beside the controls", settings.fitMapBesideControls) { settings.fitMapBesideControls = it }
        check("Minimap (tap it to see the whole level)", settings.showMinimap) { settings.showMinimap = it }
        check("Tablets / foldables: side panel with stats, map and inventory", settings.sidePanel) { settings.sidePanel = it }
        box.addView(Button(this).apply {
            text = "Reset map zoom"; isAllCaps = false
            setOnClickListener { settings.tileDp = 0f; game.tileDp = 0f; toast("Zoom reset") }
        })
        box.addView(TextView(this).apply { text = "Pinch the map to zoom in or out; the view follows your character."; textSize = 12f; alpha = 0.7f })

        header("Keyboard")
        check("Arrow keys / numpad move (hardware keyboard)", settings.arrowsAsMoves) { settings.arrowsAsMoves = it }

        header("Wild features (new games start with these; a saved game keeps its own)")
        for ((bit, label) in Settings.WILD_FEATURES) check(label, settings.wild(bit)) { settings.setWild(bit, it) }
        check("Card drop toasts: show each card as it drops", settings.cardToasts) { settings.cardToasts = it }
        check("Reduce glitch effects (no shaking or colour cycling)", settings.reduceGlitch) { settings.reduceGlitch = it }
        check("Reduce weather effects (no drifting rain or snow, no lightning flashes)", settings.reduceWeather) { settings.reduceWeather = it }
        check("Auto-photo on death / victory", settings.autoPhoto) { settings.autoPhoto = it }
        CueSettings.addTo(this, box, settings)

        AccessibilitySettingsUi.addTo(box, a11yPrefs) { applyA11y() }

        AlertDialog.Builder(this)
            .setTitle("Settings")
            .setView(ScrollView(this).apply { addView(box) })
            .setPositiveButton("OK") { _, _ ->
                settings.playerName = name.text.toString()
                settings.orientation = rotation.selectedItemPosition
                applySettings()
                applyRotationSetting()
            }
            .show()
    }

    private fun toast(s: String) = Toast.makeText(this, s, Toast.LENGTH_SHORT).show()

    private fun showUnlocked(list: List<Achievement>) {
        if (!settings.achievementToasts) return
        val text = list.joinToString("\n") { "Achievement unlocked: ${it.title}" }
        game.post { Toast.makeText(this, text, Toast.LENGTH_LONG).show() }
    }
}
