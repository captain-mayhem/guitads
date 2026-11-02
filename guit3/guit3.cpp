#ifdef RCSID
static char RCSid[] =
"$Header$";
#endif

/* 
 *   Copyright (c) 2000, 2002 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  w32t3.cpp - TADS 3 version-specific implementation
Function
  
Notes
  
Modified
  01/20/00 MJRoberts  - Creation
*/

#ifdef _WIN32
#include <WinSock2.h>
#include <Windows.h>
#else
#include "tadsplat.h"
#endif

#include "t3main.h"
#include "guimain.h"
#include "guiengine.h"
#include "hos_gui.h"
#include "guios.h"

/* include the TADS 2 VM version information */
#include "trd.h"

/* include the TADS 3 VM version information */
#include "vmvsn.h"

/* include some needed T3 headers */
#include "vmerr.h"
#include "vmimage.h"
#include "osifcnet.h"
#include "t3std.h"

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif


/* ------------------------------------------------------------------------ */
/*
 *   ask the web UI window to yield the foreground
 */
void w32_webui_yield_foreground()
{
#ifdef _WIN32
    osnet_webui_yield_foreground();
#endif
}

/*
 *   bring the web UI window to the foreground
 */
void w32_webui_to_foreground()
{
#ifdef _WIN32
    osnet_webui_to_foreground();
#endif
}

/* ------------------------------------------------------------------------ */
/*
 *   Web UI launch hook: open a Web UI game's start page in the system's
 *   default Web browser.  guit3 has no bundled tadsweb.exe (that's a
 *   separate executable built only by the classic htmltads targets, and
 *   even then it lands in a different install/output directory than
 *   guit3.exe - see win32/osnet-connect.cpp's launch_tadsweb(), which looks
 *   for tadsweb.exe next to the running executable), so registering this
 *   hook (see guimain.cpp) replaces that lookup with os_open_url() (guios.h)
 *   - ShellExecute "open" on Windows, xdg-open on Linux - which
 *   works with whatever browser the user has set as their default.
 *
 *   This gives up a few things the tadsweb.exe/named-pipe protocol
 *   provides - see the doc comment on os_webui_launch_hook_t in
 *   osifcnet.h for the details - but standard Web UI games built on
 *   lib/webui.t's getInputFile() don't depend on any of them, since that
 *   code path already has to work without them in client/server mode.
 */
int guit3_webui_launch_hook(const char *addr, int port, const char *path,
                            char **errmsg)
{
#ifdef __EMSCRIPTEN__
    /*
     *   Under Emscripten the "server" is the in-memory loopback transport
     *   (tads3/emscripten/osnetloop.h), not a real socket, so there's no
     *   URL a browser could open in another tab. Returning failure
     *   would make connectWebUI() throw and the game shut its HTTPServer
     *   straight back down. Instead, just hand the loopback port and start
     *   page path to JS: Module.webuiLaunch records them, a
     *   "guit3-webui-launch" window event tells the WebUI bridge
     *   (emscripten/guit3-webui-bridge.js, which has to start routing the
     *   start page's path to the VM before anything loads it), and an
     *   optional Module.onWebUILaunch(port, path) callback lets the page
     *   react (the overlay iframe - see step 6 of webui-emscripten-plan.md).
     */
    (void)addr;
    MAIN_THREAD_EM_ASM({
        var path = UTF8ToString($1);
        Module['webuiLaunch'] = ({ port: $0, path: path });
        window.dispatchEvent(new CustomEvent('guit3-webui-launch',
                                             { detail: { port: $0, path: path } }));
        if (typeof Module['onWebUILaunch'] === 'function')
            Module['onWebUILaunch']($0, path);
    }, port, path);

    *errmsg = 0;
    return TRUE;
#else
    char url[1024];
#ifdef _WIN32
    _snprintf(url, sizeof(url), "http://%s:%d%s", addr, port, path);
#else
    snprintf(url, sizeof(url), "http://%s:%d%s", addr, port, path);
#endif
    url[sizeof(url) - 1] = '\0';

#ifdef ANDROID
    /*
     *   On Android the page goes into guit3's own view for it: a browser in
     *   front would leave guit3 in the background, where the system may
     *   freeze or end it, the game's server with it.  Closing the view ends
     *   the input, as a closed tadsweb.exe window does.
     */
    if (gui_show_webui(url, osnet_webui_closed))
    {
        *errmsg = 0;
        return TRUE;
    }
#endif

    if (!os_open_url(url))
    {
        *errmsg = lib_copy_str(
            "Unable to open the game's start page in a Web browser");
        return FALSE;
    }

    *errmsg = 0;
    return TRUE;
#endif
}

#if defined(ANDROID) && defined(TADSNET)
/*
 *   Network wait hook (oss_set_net_wait_hook(), osifcnet.h): while the game
 *   waits for its Web UI's requests, the engine's thread still has to
 *   answer the activity (a pause, a new window), or the system ends the app
 *   within seconds.  When the activity is going away, the Web UI is closed
 *   once, so that a Web UI game ends its input and quits, rather than
 *   keeping the engine's thread in the wait while the activity waits for
 *   it.
 */
void guit3_net_wait_hook()
{
    static int closed = FALSE;
    if (gui_quit_requested())
    {
        if (!closed)
        {
            closed = TRUE;
            osnet_webui_closed();
        }
        return;
    }
    gui_pump_events();
}
#endif


/* ------------------------------------------------------------------------ */
/*
 *   list memory blocks in the T3 VM subsystem
 */
#ifdef TADSHTML_DEBUG

void th_list_subsys_memory_blocks()
{
    t3_list_memory_blocks(&os_dbg_sys_msg);
}

#endif /* TADSHTML_DEBUG */

