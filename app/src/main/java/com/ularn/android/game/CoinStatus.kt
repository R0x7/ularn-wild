package com.ularn.android.game

import java.util.Locale

/**
 * The player's LarnCoin (Wild Edition, compat/wild_coin.c) as the side
 * panel's LARNCOIN section shows it. No Android classes: unit-tested on
 * the JVM.
 */
data class CoinStatus(
    /** Coins held (> 0). */
    val held: Int,
    /** Today's price in gold. */
    val price: Int,
) {
    /** "12 LarnCoin". */
    val title: String get() = "${grouped(held.toLong())} LarnCoin"

    /** "Worth 1,476 gold (123 each)"; Long because a big stack at a high price passes Int.MAX_VALUE. */
    val worth: String get() = "Worth ${grouped(held.toLong() * price)} gold (${grouped(price.toLong())} each)"

    /** 3,700 whatever the phone's language (the game's own text is English). */
    private fun grouped(n: Long) = String.format(Locale.ROOT, "%,d", n)
}
