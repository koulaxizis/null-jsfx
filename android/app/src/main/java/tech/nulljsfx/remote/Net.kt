package tech.nulljsfx.remote

import java.util.Base64

/** Network helpers with no Android dependency, so the unit tests can run them on the JVM. */
object Net {
    /** "192.168.1.20" for the IPv4 address [ip] in network byte order (first octet high). */
    fun ipString(ip: Int): String =
        listOf(ip ushr 24, ip ushr 16, ip ushr 8, ip).joinToString(".") { (it and 0xff).toString() }

    fun ipInt(s: String): Int? {
        val p = s.split(".")
        if (p.size != 4) return null
        var v = 0
        for (o in p) {
            val n = o.toIntOrNull() ?: return null
            if (n !in 0..255) return null
            v = (v shl 8) or n
        }
        return v
    }

    /** The broadcast address of [ip] in a network of [prefix] bits. */
    fun broadcast(ip: Int, prefix: Int): Int {
        val p = prefix.coerceIn(0, 32)
        val mask = if (p == 0) 0 else -1 shl (32 - p)
        return (ip and mask) or mask.inv()
    }

    /**
     * The addresses to look for REAPER on: the other hosts of the /24 around [ip] (a larger
     * home network is scanned only in its /24, which keeps the scan to a few seconds).
     */
    fun scanHosts(ip: Int, prefix: Int): List<String> {
        val p = prefix.coerceIn(24, 30)
        val mask = -1 shl (32 - p)
        val net = ip and mask
        val last = net or mask.inv()
        return ((net + 1) until last).filter { it != ip }.map(::ipString)
    }

    /** "Basic dXNlcjpwYXNz", or null when there is no user name. */
    fun basicAuth(user: String?, pass: String?): String? {
        if (user.isNullOrEmpty()) return null
        val raw = "$user:${pass ?: ""}".toByteArray(Charsets.UTF_8)
        return "Basic " + Base64.getEncoder().encodeToString(raw)
    }

    /**
     * The REAPER URL for a request the page made to its own origin: [url] is the full asset URL
     * (https://appassets.androidplatform.net/_/TRANSPORT;TRACK), kept byte for byte after the
     * host so percent-escapes reach REAPER unchanged. Null when it is not a /_/ request.
     */
    fun reaperUrl(url: String, host: String, port: Int): String? {
        val start = url.indexOf("://")
        if (start < 0) return null
        val slash = url.indexOf('/', start + 3)
        if (slash < 0) return null
        val rest = url.substring(slash)
        if (!rest.startsWith("/_/")) return null
        val h = if (host.contains(':') && !host.startsWith("[")) "[$host]" else host
        return "http://$h:$port$rest"
    }

    /** True when version [a] (e.g. "3.1") is newer than [b] ("3.0"); tags carry no "v". */
    fun newer(a: String, b: String): Boolean {
        val x = a.trim().removePrefix("v").split(".").map { it.toIntOrNull() ?: 0 }
        val y = b.trim().removePrefix("v").split(".").map { it.toIntOrNull() ?: 0 }
        for (i in 0 until maxOf(x.size, y.size)) {
            val d = x.getOrElse(i) { 0 } - y.getOrElse(i) { 0 }
            if (d != 0) return d > 0
        }
        return false
    }
}
