# guit3 for Android

The Gradle project of guit3 on Android: an APK with guit3 (`libguit3.so`, in
Captain's Game Engine's activity) and a list of games. It plays TADS 3 games
(`.t3`); WebUI games don't work (no network module on Android).

## Building

The three checkouts sit side by side, as for the desktop build:

```
tads-runner/      its CMakeLists.txt is what Gradle builds
guitads/          this repository (guit3/android is here)
captainsengine/   Captain's Game Engine
```

Other places are set with `-Pguit3.tadsRunnerDir=...` and `-Pguit3.cgeDir=...`
(or in `gradle.properties`). Gradle needs the Android SDK with NDK
30.0.16248370 and CMake 4.1.2 (`local.properties` with `sdk.dir`, which
Android Studio writes) and a JDK 17 or later, e.g. Android Studio's:

```
$env:JAVA_HOME = "C:\Program Files\Android\Android Studio\jbr"
.\gradlew.bat assembleDebug      # app/build/outputs/apk/debug/app-debug.apk
.\gradlew.bat assembleRelease    # signed with guit3.keystore, guit3.keystorePassword,
                                 # guit3.keyAlias and guit3.keyPassword if they are set
```

The APK is for `arm64-v8a` (phones) and `x86_64` (the emulator).

## Using it

"Add a file" takes a game's `.t3` file or a `.zip` with it; "Add a folder"
takes a folder with the `.t3` file (also in a subfolder), for games with
files of their own next to it. The game is copied into the app; a long press
deletes it again, with its saved games, which are in the game's folder.

The on-screen keyboard opens while the game reads a command; a tap brings it
back after it was closed. In MORE mode a tap shows the next page. Back is the
Escape key for the game. File > Exit returns to the list of games.

How it works: section 5.34 of [../migration.md](../migration.md).
