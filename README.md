# guitads

guitads holds `guit3`, a cross-platform (Dear ImGui / GLFW) HTML TADS 3
interpreter ported from the Win32 `htmlt3` client of HTML TADS. It also holds
the HTML TADS core (sourced from GPL3 licensed QTads) and the third-party libraries guit3 needs.
It is not built
on its own: tads-runner picks it up as a sibling checkout via
`add_subdirectory()`: [tads-runner](https://github.com/captain-mayhem/tads-runner), which is licensed under GPL-2.0

See LICENSE for licensing details

## How to build

### Prerequisites to install
- Visual Studio 2022 or 2026 with C++ Compiler
- CMake (Version 4.x recommended, Version >= 3.19 required)
- git

### Directory setup
- git clone https://github.com/captain-mayhem/tads-runner.git
- git clone https://github.com/captain-mayhem/htmltads.git
- create an empty folder parallel to the two git repositories as build directory, e.g. build_tads

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