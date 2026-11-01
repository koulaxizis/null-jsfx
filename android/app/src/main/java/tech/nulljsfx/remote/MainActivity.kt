package tech.nulljsfx.remote

import android.annotation.SuppressLint
import android.app.Activity
import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.view.HapticFeedbackConstants
import android.view.View
import android.view.WindowInsets
import android.view.WindowInsetsController
import android.view.WindowManager
import android.webkit.WebResourceRequest
import android.webkit.WebResourceResponse
import android.webkit.WebView
import android.webkit.WebViewClient
import androidx.webkit.WebMessageCompat
import androidx.webkit.WebMessagePortCompat
import androidx.webkit.WebViewAssetLoader
import androidx.webkit.WebViewCompat
import androidx.webkit.WebViewFeature
import org.json.JSONObject
import java.io.ByteArrayInputStream
import java.net.HttpURLConnection
import java.net.URL

/**
 * One WebView showing android/web/core/index.html from the app's assets, at
 * https://appassets.androidplatform.net/app/. Requests the pages make to /_/ on that origin
 * are REAPER web-interface calls: they are forwarded to the REAPER the user connected to, so
 * the REAPER page (remote/reaper_www_root/null_jsfx.html) runs here unchanged.
 */
class MainActivity : Activity(), Shell.Ui {
    private lateinit var web: WebView
    private lateinit var shell: Shell
    private val main = Handler(Looper.getMainLooper())
    private var port: WebMessagePortCompat? = null

    @SuppressLint("SetJavaScriptEnabled")
    override fun onCreate(state: Bundle?) {
        super.onCreate(state)
        web = WebView(this)
        web.setBackgroundColor(0xFF1B1A18.toInt())
        setContentView(web)

        shell = Shell(this, ::post, this)
        val assets = WebViewAssetLoader.Builder()
            .addPathHandler("/app/", WebViewAssetLoader.AssetsPathHandler(this))
            .build()

        with(web.settings) {
            javaScriptEnabled = true
            domStorageEnabled = true
            allowFileAccess = false
            allowContentAccess = false
            mediaPlaybackRequiresUserGesture = true
            setSupportZoom(false)
            builtInZoomControls = false
            textZoom = 100
        }
        web.addJavascriptInterface(shell, "NullShellNative")

        web.webViewClient = object : WebViewClient() {
            override fun shouldInterceptRequest(view: WebView, req: WebResourceRequest): WebResourceResponse? {
                val url = req.url
                if (url.host == HOST && (url.encodedPath ?: "").startsWith("/_/")) return reaper(url.toString())
                return assets.shouldInterceptRequest(url)
            }

            // only the app's own pages load in the WebView; any other link opens in the browser
            override fun shouldOverrideUrlLoading(view: WebView, req: WebResourceRequest): Boolean {
                if (req.url.host == HOST) return false
                if (req.url.scheme == "https") {
                    try { startActivity(Intent(Intent.ACTION_VIEW, req.url)) } catch (e: Exception) { }
                }
                return true
            }

            override fun onPageFinished(view: WebView, url: String) {
                if (Uri.parse(url).host == HOST) openPort()
            }
        }

        if (state != null) web.restoreState(state) else web.loadUrl("https://$HOST/app/core/index.html")
    }

    /** Forwards one /_/ request to REAPER's web interface and hands back its answer. */
    private fun reaper(url: String): WebResourceResponse {
        val target = Net.reaperUrl(url, shell.reaperHost, shell.reaperPort)
        if (shell.reaperHost.isEmpty() || target == null) return text(503, "Service Unavailable", "not connected")
        return try {
            val c = URL(target).openConnection() as HttpURLConnection
            c.connectTimeout = 2500
            c.readTimeout = 4000
            c.useCaches = false
            shell.reaperAuth?.let { c.setRequestProperty("Authorization", it) }
            val code = c.responseCode
            val body = (if (code < 400) c.inputStream else c.errorStream)?.use { it.readBytes() } ?: ByteArray(0)
            c.disconnect()
            WebResourceResponse("text/plain", "utf-8", code, c.responseMessage ?: "OK",
                mapOf("Cache-Control" to "no-store"), ByteArrayInputStream(body))
        } catch (e: Exception) {
            text(502, "Bad Gateway", e.javaClass.simpleName)
        }
    }

    private fun text(code: Int, reason: String, body: String) =
        WebResourceResponse("text/plain", "utf-8", code, reason, mapOf("Cache-Control" to "no-store"),
            ByteArrayInputStream(body.toByteArray()))

    /** A fresh message channel for each page load: the page gets one end as "nullshell". */
    private fun openPort() {
        port?.close()
        port = null
        if (!WebViewFeature.isFeatureSupported(WebViewFeature.CREATE_WEB_MESSAGE_CHANNEL) ||
            !WebViewFeature.isFeatureSupported(WebViewFeature.POST_WEB_MESSAGE)) return
        val ports = WebViewCompat.createWebMessageChannel(web)
        port = ports[0]
        WebViewCompat.postWebMessage(web, WebMessageCompat("nullshell", arrayOf(ports[1])), Uri.parse("https://$HOST"))
    }

    /** Sends one JSON message to the page (from any thread). */
    private fun post(json: String) {
        main.post {
            val p = port
            if (p != null && WebViewFeature.isFeatureSupported(WebViewFeature.WEB_MESSAGE_PORT_POST_MESSAGE)) {
                p.postMessage(WebMessageCompat(json))
            } else {
                web.evaluateJavascript("window.NullShell&&NullShell._recv(${JSONObject.quote(json)})", null)
            }
        }
    }

    // --- Shell.Ui ---------------------------------------------------------------------------

    override fun keepAwake(on: Boolean) {
        main.post {
            if (on) window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
            else window.clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        }
    }

    override fun fullscreen(on: Boolean) {
        main.post {
            if (android.os.Build.VERSION.SDK_INT >= 30) {
                val c = window.insetsController ?: return@post
                if (on) {
                    c.hide(WindowInsets.Type.systemBars())
                    c.systemBarsBehavior = WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
                } else {
                    c.show(WindowInsets.Type.systemBars())
                }
            } else {
                @Suppress("DEPRECATION")
                web.systemUiVisibility = if (on) {
                    View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY or View.SYSTEM_UI_FLAG_FULLSCREEN or
                        View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                } else 0
            }
        }
    }

    override fun haptic() {
        main.post { web.performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY) }
    }

    // --- lifecycle ----------------------------------------------------------------------------

    @Deprecated("Back goes to the page first; on its first screen it leaves the app")
    override fun onBackPressed() {
        web.evaluateJavascript("window.NullRemote?NullRemote.back():false") { handled ->
            if (handled != "true") {
                @Suppress("DEPRECATION")
                super.onBackPressed()
            }
        }
    }

    override fun onSaveInstanceState(out: Bundle) {
        super.onSaveInstanceState(out)
        web.saveState(out)
    }

    override fun onDestroy() {
        shell.closeAll()
        port?.close()
        web.destroy()
        super.onDestroy()
    }

    companion object {
        const val HOST = "appassets.androidplatform.net"
    }
}
