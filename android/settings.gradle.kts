// NULL Remote for Android: one app (tech.nulljsfx.remote) with a tab per target
// (REAPER now, Mixers next). See android/README.md.
pluginManagement {
    repositories {
        google()
        mavenCentral()
        gradlePluginPortal()
    }
}
dependencyResolutionManagement {
    repositoriesMode.set(RepositoriesMode.FAIL_ON_PROJECT_REPOS)
    repositories {
        google()
        mavenCentral()
    }
}
rootProject.name = "null-remote"
include(":app")
