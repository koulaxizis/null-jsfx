// The NULL JSFX Android remotes: one shell, two apps (Christos, 2026-11-01).
//   daws    "Remote for DAWs"     tech.nulljsfx.remote.daws  (android/web/daws: REAPER first)
//   mixers  "Remote for Mixers"   tech.nulljsfx.remote.mixers (android/web/mixers)
// Each comes as "github" (our releases, with an update notice) and "fdroid". The pages live in
// android/web (bundled as assets); the REAPER page and the fonts are copied in at build time
// from remote/ and native/common/fonts, so the app and the tablet page served by REAPER are
// always the same file.
import java.util.Base64

plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

val repo: File = rootProject.projectDir.parentFile

// One version for the whole suite (Bible R19): read it from FOOTER in tests/lint.py.
val suiteVersion: String = Regex("""FOOTER = "//NULL JSFX v([0-9.]+) """)
    .find(repo.resolve("tests/lint.py").readText())?.groupValues?.get(1)
    ?: error("no FOOTER version in tests/lint.py")
// 3.0 -> 30000, 2.6.1 -> 20601: always grows with the suite version
val suiteCode: Int = suiteVersion.split(".").map(String::toInt).let { p ->
    p[0] * 10000 + p.getOrElse(1) { 0 } * 100 + p.getOrElse(2) { 0 }
}

// Release signing: only from the environment (GitHub secrets), never from a file in the repo.
val keystoreB64: String? = System.getenv("NULL_REMOTE_KEYSTORE")?.takeIf { it.isNotBlank() }

android {
    namespace = "tech.nulljsfx.remote"
    compileSdk = 35

    defaultConfig {
        minSdk = 26
        targetSdk = 35
        versionCode = suiteCode
        versionName = suiteVersion
    }

    flavorDimensions += listOf("app", "store")
    productFlavors {
        // released identities: the application ids never change (Bible R3)
        create("daws") {
            dimension = "app"
            applicationId = "tech.nulljsfx.remote.daws"
            buildConfigField("String", "APP", "\"daws\"")
        }
        create("mixers") {
            dimension = "app"
            applicationId = "tech.nulljsfx.remote.mixers"
            buildConfigField("String", "APP", "\"mixers\"")
        }
        // the APK on our GitHub releases (Obtainium reads these too): tells the user about updates
        create("github") {
            dimension = "store"
            buildConfigField("boolean", "UPDATE_CHECK", "true")
        }
        // F-Droid and Droidify build and update it themselves: no update notice of our own
        create("fdroid") {
            dimension = "store"
            buildConfigField("boolean", "UPDATE_CHECK", "false")
        }
    }

    signingConfigs {
        if (keystoreB64 != null) {
            create("release") {
                val f = layout.buildDirectory.file("release.keystore").get().asFile
                f.parentFile.mkdirs()
                f.writeBytes(Base64.getDecoder().decode(keystoreB64.trim()))
                storeFile = f
                storePassword = System.getenv("NULL_REMOTE_KEYSTORE_PASSWORD")
                keyAlias = System.getenv("NULL_REMOTE_KEY_ALIAS")
                keyPassword = System.getenv("NULL_REMOTE_KEY_PASSWORD")
            }
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            if (keystoreB64 != null) signingConfig = signingConfigs.getByName("release")
        }
    }

    buildFeatures { buildConfig = true }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    kotlinOptions { jvmTarget = "17" }

    sourceSets {
        getByName("main") { assets.srcDir(repo.resolve("android/web")) }
    }

    lint {
        abortOnError = true
        warningsAsErrors = false
    }

    // reproducible output for F-Droid: no build-time metadata in the APK
    dependenciesInfo {
        includeInApk = false
        includeInBundle = false
    }
}

// The REAPER page (remote/reaper_www_root/null_jsfx.html), the Noto Sans fonts and the web version
// of the plugins (web/runtime and web/fx, as nullfx/: the DAW tabs draw each NULL JSFX plugin with
// its own GFX interface), copied into the assets at build time; never committed twice (Bible R14).
abstract class SharedAssets : DefaultTask() {
    @get:InputFile abstract val page: RegularFileProperty
    @get:InputDirectory abstract val fonts: DirectoryProperty
    @get:InputDirectory abstract val web: DirectoryProperty
    @get:OutputDirectory abstract val out: DirectoryProperty

    @TaskAction
    fun copy() {
        val root = out.get().asFile
        root.deleteRecursively()
        page.get().asFile.copyTo(root.resolve("daws/reaper/null_jsfx.html"), true)
        for (name in listOf("NotoSans-Regular.ttf", "NotoSans-Bold.ttf")) {
            fonts.get().asFile.resolve(name).copyTo(root.resolve("core/fonts/$name"), true)
        }
        val w = web.get().asFile
        for (name in listOf("eel-rt.js", "null-fx.js", "null-gfx.js")) {
            w.resolve("runtime/$name").copyTo(root.resolve("nullfx/runtime/$name"), true)
        }
        w.resolve("fx").listFiles { f -> f.name.endsWith(".js") || f.name == "index.json" }!!.forEach {
            it.copyTo(root.resolve("nullfx/fx/${it.name}"), true)
        }
    }
}

val sharedAssets = tasks.register<SharedAssets>("sharedAssets") {
    page.set(repo.resolve("remote/reaper_www_root/null_jsfx.html"))
    fonts.set(repo.resolve("native/common/fonts"))
    web.set(repo.resolve("web"))
}

androidComponents {
    onVariants { variant ->
        variant.sources.assets?.addGeneratedSourceDirectory(sharedAssets, SharedAssets::out)
    }
}

dependencies {
    implementation("androidx.webkit:webkit:1.12.1")
    testImplementation("junit:junit:4.13.2")
}
