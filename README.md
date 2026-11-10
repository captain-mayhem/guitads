# guitads

[![build](https://github.com/captain-mayhem/guitads/actions/workflows/build.yml/badge.svg)](https://github.com/captain-mayhem/guitads/actions/workflows/build.yml)

guitads holds `guit3`, a cross-platform (Dear ImGui) HTML TADS 3
interpreter ported from the Win32 `htmlt3` client of HTML TADS. It also holds
the HTML TADS core (sourced from GPL3 licensed QTads) and the third-party libraries only guit3 needs
(libmng, and TinySoundFont for MIDI music, which guit3 plays with the bundled General MIDI soundfont
TimGM6mb in `guit3/soundfont/`).
It is not built
on its own: tads-runner picks it up as a sibling checkout via
`add_subdirectory()`: [tads-runner](https://github.com/captain-mayhem/tads-runner), which is licensed under GPL-2.0

guit3 runs on [Captain's Game Engine](https://github.com/captain-mayhem/captainsengine)
(LGPL 2.1), a third sibling checkout: the engine provides the window, the graphics, the main loop and
Dear ImGui, the sound (playback, and the WAV, MP3 and Ogg Vorbis decoders), and zlib, libpng, libjpeg and FreeType. guit3 links the engine
library (`CGE.dll` and `CGE_GL.dll` next to `guit3.exe` on Windows, `lib/` on Linux) and reads the
engine's `engine.ini` next to its executable. guitads finds it in `../captainsengine`; set `CGE_DIR` to use another folder.

A web version can be found here:
[Guitads Web](https://captain-mayhem.github.io/guitads)
You can load your Tads 3 game with "File", "Open Game from Device..."
Save games are stored in the browser's persistent storage, you can download/restore your save games with
"Export saved position" / "Import saved position".

See LICENSE for licensing details

## How to build

### Prerequisites to install
- Visual Studio 2022 or 2026 with C++ Compiler
- CMake (Version 4.x recommended, Version >= 3.21 required)
- git

### Directory setup
- git clone https://github.com/captain-mayhem/tads-runner.git
- git clone https://github.com/captain-mayhem/guitads.git
- git clone https://github.com/captain-mayhem/captainsengine.git
- create an empty folder parallel to the three git repositories as build directory, e.g. build_tads

`dependencies.txt` pins the tads-runner and captainsengine commits that belong to a guitads commit (only
guitads is tagged for releases). Configuring warns when your checkouts differ from it. To build exactly
the pinned commits, e.g. for an older release:
- check out the guitads commit or tag
- `cmake -P guitads/cmake/deps.cmake checkout` (`status` shows pinned vs. checked out)

### Making a release
- set the new version in `GUITADS_VERSION` (`CMakeLists.txt`); it names the `GuiTads-<version>-<system>.zip`
  package, and the release workflow checks that the tag is `v<version>`
- commit and push tads-runner and captainsengine
- `cmake -P guitads/cmake/deps.cmake update`, which writes their current commits to `dependencies.txt`
  (it refuses while either has uncommitted changes or unpushed commits)
- commit `dependencies.txt`, then tag guitads (`v*`) and push the tag; the release workflow builds the
  pinned commits, and fails if they don't match

### Build instructions
- Open a Visual Studio "x64 Native Tools Command Prompt"
- enter your build directory (cd build_tads)
- Call CMake: cmake -DCMAKE_INSTALL_PREFIX=install ..\tads-runner
- This will create a Tads.sln solution file in your build Directory
- Double click on it to open Visual Studio
- In the drop down, switch from "Debug" to "Release" (unless you want to build a Debug version)
- Build solution
- Build the target INSTALL to create the folder named install in the build directory that contains the build result
- To create a zip file, you can build the PACKAGE target