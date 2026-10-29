# This is the guit3-only sibling of the old htmltads repo: it holds just the
# HTML TADS core sources guit3 (the cross-platform Dear ImGui interpreter on
# Captain's Game Engine) needs, plus the GPLv3-sourced (via QTads) htmltads/
# core files, and drops everything specific to the legacy Win32 htmlt3/
# htmltdb3/tadsweb targets (scintilla/, textindex/, t3doc/, wbaddons/, and
# the Win32 resource chain rooted at win32/htmlt3.rc - see guit3/CMakeLists.txt's
# WIN32 NOTE). The one exception is htmltads/win32/hos_w32.h: the shared
# tads3/CMakeLists.txt's t3htm/t3htm_d targets need it on native Windows
# builds (T_WIN32 without IMGUI routes html_os.h's switchboard there) even
# though guit3 itself uses guios_w32.cpp, not hos_w32.h, for its own code.
#
# Like the old htmltads repo, this is not built standalone - it's a sibling
# checkout to tads-runner, which auto-detects ../guitads and/or ../htmltads
# and pulls them in via add_subdirectory(). When both are present, this repo
# is added first and owns guit3 and the 3rdparty libraries; htmltads then
# skips those and only adds its legacy htmlt3/htmltdb3/tadsweb targets. See
# tads-runner's CMakeLists.txt and CLAUDE.md.

# Captain's Game Engine is a third sibling checkout. guit3 runs on it (the
# window, the graphics, the main loop, Dear ImGui), and it brings the shared
# 3rdparty libraries (zlib, libpng, libjpeg, FreeType, Dear ImGui) as
# targets, and decodes and plays guit3's sounds; libmng stays here. It is added
# once, before anything here uses those targets.
if (NOT TARGET CGE::Engine)
	set(CGE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../captainsengine" CACHE PATH "Path to Captain's Game Engine")
	if (NOT EXISTS "${CGE_DIR}/CMakeLists.txt")
		message(FATAL_ERROR "Captain's Game Engine not found at ${CGE_DIR} - set CGE_DIR")
	endif()
	# guit3 waits for input in nested loops and uses threads, so on the web
	# the engine is built in its threaded configuration (pthreads, ASYNCIFY)
	if (EMSCRIPTEN)
		set(CGE_WEB_THREADS ON)
	endif()
	add_subdirectory(${CGE_DIR} cge)
endif()

add_subdirectory(libmng)

add_subdirectory(guit3)
