// The Android project of guit3: an APK or AAB with guit3's native library
// (built by CMake from tads-runner's CMakeLists.txt, which adds guitads and
// Captain's Game Engine), the engine's activity and a launcher activity with
// the list of games. See app/build.gradle.kts and README.md.

pluginManagement {
    repositories {
        google()
        mavenCentral()
        gradlePluginPortal()
    }
}

dependencyResolutionManagement {
    repositoriesMode = RepositoriesMode.FAIL_ON_PROJECT_REPOS
    repositories {
        google()
        mavenCentral()
    }
}

rootProject.name = "GuiT3"
include(":app")
