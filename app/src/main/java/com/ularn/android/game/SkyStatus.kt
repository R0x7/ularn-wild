package com.ularn.android.game

import java.util.Locale

/**
 * The town's clock and sky (Wild Edition, compat/wild_weather.c) as the side
 * panel's SKY section shows it. Everything is worked out from the game clock
 * the same way the game does it (50 turns an hour, a new game starts at
 * 08:00, night is 21:00-05:59; see [WeatherMath]). No Android classes:
 * unit-tested on the JVM.
 */
data class SkyStatus(
    /** The game's turn counter (S->game_time). */
    val gameTime: Int,
    /** Weather state, [WeatherMath.CLEAR] .. [WeatherMath.STORM]. */
    val weather: Int,
    /** The player is in town, under the sky: only there is the weather known. */
    val outside: Boolean,
) {
    private val t = maxOf(0, gameTime).toLong()
    private val hours = t / WeatherMath.TURNS_PER_HOUR + WeatherMath.DAY_START_HOUR

    /** 0-23, as hour_now() in wild_weather.c. */
    val hour: Int get() = (hours % 24).toInt()

    /** 0-59: a turn is 1.2 minutes. */
    val minute: Int get() = ((t % WeatherMath.TURNS_PER_HOUR) * 60 / WeatherMath.TURNS_PER_HOUR).toInt()

    /** Day 1 is the day the adventure starts; a new day starts at midnight. */
    val day: Int get() = (hours / 24 + 1).toInt()

    val isNight: Boolean get() = WeatherMath.isNight(hour)

    /** "Day 2, 14:24 · Rain"; underground the weather is left out (you
     *  only hear about it when you climb back out). */
    val title: String get() {
        val clock = String.format(Locale.ROOT, "Day %d, %02d:%02d", day, hour, minute)
        return if (outside) "$clock · ${weatherName()}" else clock
    }

    private fun weatherName() = when (weather) {
        WeatherMath.RAIN -> "Rain"
        WeatherMath.SNOW -> "Snow"
        WeatherMath.STORM -> "Storm"
        else -> if (isNight) "Clear night" else "Clear"
    }

    /** Turns until the next dusk (by day) or dawn (by night). */
    val turnsToChange: Int get() {
        val target = if (isNight) WeatherMath.NIGHT_END else WeatherMath.NIGHT_START
        val hoursAhead = Math.floorMod(target - hour, 24)
        return (hoursAhead * WeatherMath.TURNS_PER_HOUR - t % WeatherMath.TURNS_PER_HOUR).toInt()
    }

    /** "Night falls in 335 turns" / "Dawn breaks in 1 turn". */
    val outlook: String get() {
        val n = turnsToChange
        val turns = String.format(Locale.ROOT, "%,d %s", n, if (n == 1) "turn" else "turns")
        return if (isNight) "Dawn breaks in $turns" else "Night falls in $turns"
    }
}
