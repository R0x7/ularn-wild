package com.ularn.android.game

import org.junit.Assert.assertEquals
import org.junit.Test

class CoinStatusTest {
    @Test fun wording() {
        val c = CoinStatus(12, 123)
        assertEquals("12 LarnCoin", c.title)
        assertEquals("Worth 1,476 gold (123 each)", c.worth)
    }

    @Test fun worthDoesNotOverflow() {
        assertEquals("Worth 4,000,000,000 gold (2,000 each)", CoinStatus(2_000_000, 2_000).worth)
    }
}
