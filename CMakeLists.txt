# This is the guit3-only sibling of the old htmltads repo: it vendors just the
# 3rdparty libraries and HTML TADS core sources guit3 (the cross-platform Dear
# ImGui/GLFW interpreter) needs, plus the GPLv3-sourced (via QTads) htmltads/
# core files, and drops everything specific to the legacy Win32 htmlt3/
# htmltdb3/tadsweb targets (scintilla/, textindex/, t3doc/, wbaddons/, and
# the Win32 resource chain rooted at win32/htmlt3.rc - see guit3/CMakeLists.txt's
# WIN32 NOTE). The one exception is htmltads/win32/hos_w32.h: the shared
# tads3/CMakeLists.txt's t3htm/t3htm_d targets need it on native Windows
# builds (T_WIN32 without IMGUI routes html_os.h's switchboard there) even
# though guit3 itself uses guios_w32.cpp, not hos_w32.h, for its own code.
#
# Like the old htmltads repo, this is not built standalone - it's a sibling
# checkout to tads-runner, which auto-detects ../guitads (preferred) or
# ../htmltads and pulls it in via add_subdirectory(). See tads-runner's
# CMakeLists.txt and CLAUDE.md.

add_subdirectory(zlib)
add_subdirectory(libpng)
add_subdirectory(jpeg)
add_subdirectory(libmng)

if (NOT EMSCRIPTEN)
	# GLFW has no Emscripten backend of its own to build - under that
	# platform, Emscripten emulates the GLFW API itself via the
	# -sUSE_GLFW=3 link flag (set on guit3 itself, see guit3/CMakeLists.txt),
	# so there is no vendored library to add here.
	set(GLFW_LIBRARY_TYPE "STATIC")
	add_subdirectory(glfw)
endif()

set(BUILD_SHARED_LIBS OFF)
set(SKIP_INSTALL_ALL ON)
add_subdirectory(freetype)

add_subdirectory(imgui)

# libogg/libvorbis back guit3's Ogg Vorbis audio decoder (tadsvorb.cpp) on
# every platform.
add_subdirectory(libogg)
add_subdirectory(libvorbis)

add_subdirectory(guit3)
