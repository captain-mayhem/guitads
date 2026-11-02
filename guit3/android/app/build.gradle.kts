import java.io.ByteArrayOutputStream
import javax.inject.Inject

plugins {
    id("com.android.application")
}

// guit3 is built as part of tads-runner (its CMakeLists.txt adds the sibling
// checkouts guitads and Captain's Game Engine), so the three checkouts sit
// next to each other; -Pguit3.tadsRunnerDir=... and -Pguit3.cgeDir=... (or
// gradle.properties) point elsewhere.
val guit3Dir: File = rootDir.resolve("..").canonicalFile
val tadsRunnerDir: File = providers.gradleProperty("guit3.tadsRunnerDir").map { file(it) }
    .getOrElse(guit3Dir.resolve("../../tads-runner")).canonicalFile
val cgeDir: File = providers.gradleProperty("guit3.cgeDir").map { file(it) }
    .getOrElse(guit3Dir.resolve("../../captainsengine")).canonicalFile

// The release signing key comes from Gradle properties, e.g. in
// ~/.gradle/gradle.properties; without them the release build is unsigned.
val releaseKeystore = providers.gradleProperty("guit3.keystore")

android {
    namespace = "de.captain_online.guit3"
    compileSdk = 37
    ndkVersion = "30.0.16248370"

    defaultConfig {
        applicationId = "de.captain_online.guit3"
        minSdk = 24
        targetSdk = 36
        versionCode = 1
        versionName = "0.1"
        ndk {
            abiFilters += listOf("arm64-v8a", "x86_64")
        }
        externalNativeBuild {
            cmake {
                // the engine and guit3 share STL objects
                arguments += listOf("-DANDROID_STL=c++_shared", "-DCGE_DIR=$cgeDir")
                targets += "guit3"
            }
        }
    }

    externalNativeBuild {
        cmake {
            path = tadsRunnerDir.resolve("CMakeLists.txt")
            // the SDK's CMake, which comes with Ninja; Gradle installs it
            version = "4.1.2"
        }
    }

    sourceSets {
        getByName("main") {
            // de.captain.cge.CgeActivity
            java.srcDir(cgeDir.resolve("Engine/android/java"))
        }
    }

    signingConfigs {
        if (releaseKeystore.isPresent) {
            create("release") {
                storeFile = file(releaseKeystore.get())
                storePassword = providers.gradleProperty("guit3.keystorePassword").orNull
                keyAlias = providers.gradleProperty("guit3.keyAlias").orNull
                keyPassword = providers.gradleProperty("guit3.keyPassword").orNull
            }
        }
    }

    buildTypes {
        getByName("release") {
            isMinifyEnabled = false
            if (releaseKeystore.isPresent)
                signingConfig = signingConfigs.getByName("release")
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
}

// The files guit3 reads as the engine's assets (CGE::Assets): its
// engine.ini and the fonts (fonts/: the engine's, and the fonts the web build
// ships, which the engine's font lookup indexes along with /system/fonts).
// And in runtime/ the files that are next to the desktop executable, which
// guit3 reads as plain files: the launcher copies them into the app's
// storage (LauncherActivity).
abstract class CollectAssets : DefaultTask() {
    @get:InputFile
    abstract val engineIni: RegularFileProperty

    @get:InputDirectory
    abstract val engineFonts: DirectoryProperty

    @get:InputDirectory
    abstract val fonts: DirectoryProperty

    @get:InputFile
    abstract val charmap: RegularFileProperty

    @get:InputDirectory
    abstract val soundfont: DirectoryProperty

    @get:OutputDirectory
    abstract val outputDir: DirectoryProperty

    @get:Inject
    abstract val fs: FileSystemOperations

    @TaskAction
    fun collect() {
        fs.sync {
            into(outputDir)
            // guit3's fonts replace the engine's files of the same name
            duplicatesStrategy = DuplicatesStrategy.INCLUDE
            from(engineIni)
            from(engineFonts) { into("fonts") }
            from(fonts) { into("fonts") }
            from(charmap) { into("runtime/charmap") }
            from(soundfont) { into("runtime/soundfont") }
        }
    }
}

// runtime/guit3.t3r, guit3's private resources (the About box's picture):
// what make_t3r() builds with t3res for the desktop, which an Android build
// can't run on the build machine. A T3 image file with one MRES block, as
// CHtmlSys_mainwin::load_exe_resources() (htmlgui.cpp) reads it.
abstract class WriteResources : DefaultTask() {
    @get:InputFile
    abstract val image: RegularFileProperty

    @get:Input
    abstract val resourceName: Property<String>

    @get:OutputDirectory
    abstract val outputDir: DirectoryProperty

    @TaskAction
    fun write() {
        val data = image.get().asFile.readBytes()
        val name = resourceName.get().toByteArray(Charsets.US_ASCII)
        val out = ByteArrayOutputStream()
        fun u2(v: Int) { out.write(v and 0xFF); out.write((v shr 8) and 0xFF) }
        fun u4(v: Int) { u2(v and 0xFFFF); u2(v ushr 16) }
        fun block(type: String, size: Int, flags: Int) {
            out.write(type.toByteArray(Charsets.US_ASCII)); u4(size); u2(flags)
        }
        // the header: signature, version 1, 32 reserved bytes, the timestamp
        // (a fixed one, so the file only changes with the picture)
        out.write("T3-image\r\n\u001a".toByteArray(Charsets.US_ASCII))
        u2(1)
        out.write(ByteArray(32))
        out.write("Sat Jan 01 00:00:00 2000".toByteArray(Charsets.US_ASCII))
        // the resources: their number, a table of contents (the offset
        // from the block's data, the size, the name XOR 0xFF), the data
        val toc = 2 + 9 + name.size
        block("MRES", toc + data.size, 0)
        u2(1)
        u4(toc)
        u4(data.size)
        out.write(name.size)
        out.write(ByteArray(name.size) { (name[it].toInt() xor 0xFF).toByte() })
        out.write(data)
        block("EOF ", 0, 1)
        val file = outputDir.get().file("runtime/guit3.t3r").asFile
        file.parentFile.mkdirs()
        file.writeBytes(out.toByteArray())
    }
}

val collectAssets = tasks.register<CollectAssets>("collectAssets") {
    engineIni = guit3Dir.resolve("engine.ini")
    engineFonts = cgeDir.resolve("Engine/fonts")
    fonts = guit3Dir.resolve("emscripten/fonts")
    charmap = tadsRunnerDir.resolve("tads3/charmap/cmaplib.t3r")
    soundfont = guit3Dir.resolve("soundfont")
}

val writeResources = tasks.register<WriteResources>("writeResources") {
    image = guit3Dir.resolve("about3.jpg")
    resourceName = "about.jpg"
}

androidComponents {
    onVariants { variant ->
        variant.sources.assets?.addGeneratedSourceDirectory(collectAssets, CollectAssets::outputDir)
        variant.sources.assets?.addGeneratedSourceDirectory(writeResources, WriteResources::outputDir)
    }
}
