package tech.nulljsfx.remote

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class NetTest {
    private val phone = Net.ipInt("192.168.1.20")!!

    @Test fun ipRoundTrip() {
        assertEquals("192.168.1.20", Net.ipString(phone))
        assertEquals("10.0.0.255", Net.ipString(Net.ipInt("10.0.0.255")!!))
        assertNull(Net.ipInt("192.168.1"))
        assertNull(Net.ipInt("192.168.1.256"))
    }

    @Test fun broadcast() {
        assertEquals("192.168.1.255", Net.ipString(Net.broadcast(phone, 24)))
        assertEquals("192.168.255.255", Net.ipString(Net.broadcast(phone, 16)))
        assertEquals("192.168.1.23", Net.ipString(Net.broadcast(phone, 30)))
    }

    @Test fun scanHostsStayInTheSlash24() {
        val hosts = Net.scanHosts(phone, 16)
        assertEquals(253, hosts.size) // .1 to .254 without the phone itself
        assertEquals("192.168.1.1", hosts.first())
        assertEquals("192.168.1.254", hosts.last())
        assertFalse("192.168.1.20" in hosts)
        assertEquals(listOf("192.168.1.21", "192.168.1.22"), Net.scanHosts(phone, 30))
    }

    @Test fun reaperUrlKeepsTheEscapes() {
        val page = "https://appassets.androidplatform.net/_/SET/EXTSTATE/NULL_JSFX_REMOTE/in_1/12%3Bview%2C1;TRANSPORT"
        assertEquals("http://192.168.1.5:8080/_/SET/EXTSTATE/NULL_JSFX_REMOTE/in_1/12%3Bview%2C1;TRANSPORT",
            Net.reaperUrl(page, "192.168.1.5", 8080))
        assertEquals("http://[fe80::1]:9000/_/TRACK", Net.reaperUrl("https://x/_/TRACK", "fe80::1", 9000))
        assertNull(Net.reaperUrl("https://appassets.androidplatform.net/app/core/index.html", "h", 1))
    }

    @Test fun basicAuth() {
        assertEquals("Basic dXNlcjpwYXNz", Net.basicAuth("user", "pass"))
        assertNull(Net.basicAuth("", "pass"))
    }

    @Test fun versions() {
        assertTrue(Net.newer("3.1", "3.0"))
        assertTrue(Net.newer("3.0.1", "3.0"))
        assertTrue(Net.newer("10.0", "9.9"))
        assertFalse(Net.newer("3.0", "3.0"))
        assertFalse(Net.newer("2.9", "3.0"))
    }
}
