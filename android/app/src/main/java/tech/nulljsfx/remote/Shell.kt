package tech.nulljsfx.remote

import android.content.Context
import android.content.Intent
import android.net.ConnectivityManager
import android.net.Uri
import android.net.wifi.WifiManager
import android.util.Base64
import android.webkit.JavascriptInterface
import org.json.JSONArray
import org.json.JSONObject
import java.net.DatagramPacket
import java.net.DatagramSocket
import java.net.HttpURLConnection
import java.net.Inet4Address
import java.net.InetAddress
import java.net.InetSocketAddress
import java.net.URL
import java.util.concurrent.ConcurrentHashMap
import java.util.concurrent.Executors
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicInteger

/**
 * What the pages can ask of the phone, as window.NullShellNative (android/web/core/shell.js
 * wraps it as window.NullShell). Replies and incoming packets go back as JSON messages through
 * [post] ({type: "udp" | "scan" | ...}).
 *
 * Every method runs on WebView's JavaBridge thread, so nothing here may block for long: network
 * work goes to [io].
 */
class Shell(
    private val context: Context,
    private val post: (String) -> Unit,
    private val ui: Ui,
) {
    interface Ui {
        fun keepAwake(on: Boolean)
        fun fullscreen(on: Boolean)
        fun haptic()
    }

    private val io = Executors.newCachedThreadPool()

    // --- REAPER: the target of the /_/ proxy (MainActivity.shouldInterceptRequest) ---------

    @Volatile var reaperHost: String = ""
        private set
    @Volatile var reaperPort: Int = 8080
        private set
    @Volatile var reaperAuth: String? = null
        private set

    /** {host, port, user, pass}: where the page's /_/ requests go from now on. */
    @JavascriptInterface
    fun reaperSet(json: String) {
        val o = JSONObject(json)
        reaperHost = o.optString("host").trim()
        reaperPort = o.optInt("port", 8080).takeIf { it in 1..65535 } ?: 8080
        reaperAuth = Net.basicAuth(o.optString("user"), o.optString("pass"))
    }

    /**
     * Looks for REAPER's web interface on [port] across the local /24 and answers with
     * {type: "scan", id, hosts: [{host, auth}], self} once every address has been tried.
     */
    @JavascriptInterface
    fun scan(id: Int, port: Int) {
        io.execute {
            val (ip, prefix) = localIpv4() ?: (0 to 0)
            val found = JSONArray()
            if (ip != 0) {
                val pool = Executors.newFixedThreadPool(48)
                for (h in Net.scanHosts(ip, prefix)) pool.execute {
                    val code = probe("http://$h:$port/_/TRANSPORT")
                    if (code == 200 || code == 401) synchronized(found) {
                        found.put(JSONObject().put("host", h).put("auth", code == 401))
                    }
                }
                pool.shutdown()
                pool.awaitTermination(20, TimeUnit.SECONDS)
            }
            post(JSONObject().put("type", "scan").put("id", id).put("hosts", found)
                .put("self", if (ip != 0) Net.ipString(ip) else "").toString())
        }
    }

    /** 200 for REAPER (its TRANSPORT answer), 401 when it asks for a password, else -1. */
    private fun probe(url: String): Int = try {
        val c = URL(url).openConnection() as HttpURLConnection
        c.connectTimeout = 400
        c.readTimeout = 600
        c.useCaches = false
        val code = c.responseCode
        val ok = code == 200 && c.inputStream.bufferedReader().use { it.readText() }.startsWith("TRANSPORT\t")
        c.disconnect()
        if (ok) 200 else if (code == 401) 401 else -1
    } catch (e: Exception) {
        -1
    }

    /** The phone's own IPv4 address and prefix length on the active network. */
    private fun localIpv4(): Pair<Int, Int>? {
        val cm = context.getSystemService(ConnectivityManager::class.java) ?: return null
        val lp = cm.getLinkProperties(cm.activeNetwork) ?: return null
        val la = lp.linkAddresses.firstOrNull { it.address is Inet4Address } ?: return null
        val b = la.address.address
        val ip = ((b[0].toInt() and 0xff) shl 24) or ((b[1].toInt() and 0xff) shl 16) or
            ((b[2].toInt() and 0xff) shl 8) or (b[3].toInt() and 0xff)
        return ip to la.prefixLength
    }

    // --- UDP (OSC for the mixers) -----------------------------------------------------------

    private class Udp(val socket: DatagramSocket, val thread: Thread)

    private val udps = ConcurrentHashMap<Int, Udp>()
    private val nextUdp = AtomicInteger(1)
    private var multicast: WifiManager.MulticastLock? = null

    /**
     * Opens a UDP socket on [localPort] (0 = any) that may broadcast. Returns its id, or -1.
     * Every packet it receives is posted as {type: "udp", id, host, port, data: base64}.
     */
    @JavascriptInterface
    fun udpOpen(localPort: Int): Int = try {
        val s = DatagramSocket(null as java.net.SocketAddress?)
        s.reuseAddress = true
        s.broadcast = true
        s.bind(InetSocketAddress(localPort.coerceIn(0, 65535)))
        val id = nextUdp.getAndIncrement()
        val t = Thread({
            val buf = ByteArray(65536)
            while (!s.isClosed) {
                try {
                    val p = DatagramPacket(buf, buf.size)
                    s.receive(p)
                    val data = Base64.encodeToString(p.data, p.offset, p.length, Base64.NO_WRAP)
                    post(JSONObject().put("type", "udp").put("id", id)
                        .put("host", p.address.hostAddress).put("port", p.port)
                        .put("data", data).toString())
                } catch (e: Exception) {
                    if (s.isClosed) break
                }
            }
        }, "udp-$id")
        t.isDaemon = true
        udps[id] = Udp(s, t)
        t.start()
        synchronized(this) {
            if (multicast == null) {
                multicast = context.applicationContext.getSystemService(WifiManager::class.java)
                    ?.createMulticastLock("null-remote")?.apply { setReferenceCounted(false); acquire() }
            }
        }
        id
    } catch (e: Exception) {
        -1
    }

    /** Sends [b64] (the raw packet, base64) from socket [id] to [host]:[port]. */
    @JavascriptInterface
    fun udpSend(id: Int, host: String, port: Int, b64: String): Boolean {
        val u = udps[id] ?: return false
        val bytes = try { Base64.decode(b64, Base64.DEFAULT) } catch (e: Exception) { return false }
        io.execute {
            try {
                u.socket.send(DatagramPacket(bytes, bytes.size, InetAddress.getByName(host), port))
            } catch (e: Exception) {
                post(JSONObject().put("type", "udperror").put("id", id).put("error", e.toString()).toString())
            }
        }
        return true
    }

    /** Sends [b64] to every host of the local network on [port] (mixer discovery). */
    @JavascriptInterface
    fun udpBroadcast(id: Int, port: Int, b64: String): Boolean {
        val target = localIpv4()?.let { (ip, prefix) -> Net.ipString(Net.broadcast(ip, prefix)) } ?: "255.255.255.255"
        return udpSend(id, target, port, b64)
    }

    @JavascriptInterface
    fun udpClose(id: Int) {
        udps.remove(id)?.socket?.close()
        synchronized(this) {
            if (udps.isEmpty()) {
                multicast?.release()
                multicast = null
            }
        }
    }

    // --- TCP (consoles whose native protocol is a TCP stream) ---------------------------------

    private val tcps = ConcurrentHashMap<Int, java.net.Socket>()

    /**
     * Connects to [host]:[port] in the background and returns the connection id at once. Posts
     * {type: "tcpopen", id} when connected, {type: "tcp", id, data: base64} for every chunk read,
     * and {type: "tcpclose", id, error} when it ends or fails. [timeoutMs] limits the connect.
     */
    @JavascriptInterface
    fun tcpOpen(host: String, port: Int, timeoutMs: Int): Int {
        val id = nextUdp.getAndIncrement()
        io.execute {
            val s = java.net.Socket()
            var error = ""
            try {
                s.tcpNoDelay = true
                s.connect(InetSocketAddress(host, port), timeoutMs.coerceIn(500, 30000))
                tcps[id] = s
                post(JSONObject().put("type", "tcpopen").put("id", id).toString())
                val buf = ByteArray(65536)
                val input = s.getInputStream()
                while (true) {
                    val n = input.read(buf)
                    if (n < 0) break
                    post(JSONObject().put("type", "tcp").put("id", id)
                        .put("data", Base64.encodeToString(buf, 0, n, Base64.NO_WRAP)).toString())
                }
            } catch (e: Exception) {
                if (!s.isClosed) error = e.toString()
            } finally {
                tcps.remove(id)
                try { s.close() } catch (e: Exception) { }
                post(JSONObject().put("type", "tcpclose").put("id", id).put("error", error).toString())
            }
        }
        return id
    }

    /** Writes [b64] (raw bytes, base64) to connection [id]; false when it is not open. */
    @JavascriptInterface
    fun tcpSend(id: Int, b64: String): Boolean {
        val s = tcps[id] ?: return false
        val bytes = try { Base64.decode(b64, Base64.DEFAULT) } catch (e: Exception) { return false }
        io.execute {
            try {
                synchronized(s) { s.getOutputStream().apply { write(bytes); flush() } }
            } catch (e: Exception) {
                try { s.close() } catch (x: Exception) { }
            }
        }
        return true
    }

    @JavascriptInterface
    fun tcpClose(id: Int) {
        tcps.remove(id)?.let { try { it.close() } catch (e: Exception) { } }
    }

    fun closeAll() {
        for (id in tcps.keys.toList()) tcpClose(id)
        for (id in udps.keys.toList()) udpClose(id)
        io.shutdownNow()
    }

    // --- the phone ----------------------------------------------------------------------------

    /** {app: "daws" | "mixers", version, code, flavor, updateCheck, sdk} */
    @JavascriptInterface
    fun info(): String = JSONObject()
        .put("app", BuildConfig.APP)
        .put("version", BuildConfig.VERSION_NAME)
        .put("code", BuildConfig.VERSION_CODE)
        .put("flavor", BuildConfig.FLAVOR_store)
        .put("updateCheck", BuildConfig.UPDATE_CHECK)
        .put("sdk", android.os.Build.VERSION.SDK_INT)
        .toString()

    @JavascriptInterface
    fun keepAwake(on: Boolean) = ui.keepAwake(on)

    @JavascriptInterface
    fun fullscreen(on: Boolean) = ui.fullscreen(on)

    @JavascriptInterface
    fun haptic() = ui.haptic()

    /** Opens an https link (the update download, the Ko-fi page) in the phone's browser. */
    @JavascriptInterface
    fun openUrl(url: String) {
        if (!url.startsWith("https://")) return
        val i = Intent(Intent.ACTION_VIEW, Uri.parse(url)).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
        try { context.startActivity(i) } catch (e: Exception) { }
    }
}
