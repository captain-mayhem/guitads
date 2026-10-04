/*
 *   guios_portable.cpp - non-Windows backend for the guit3 OS-service hooks
 *   (guios.h)
 *
 *   This is the cross-platform counterpart of guios_w32.cpp: same hooks, no
 *   <windows.h>.  CMake selects exactly one backend per build - guios_w32.cpp
 *   on WIN32, this file everywhere else (see htmltads/imgui/CMakeLists.txt).
 *
 *   Coverage as of M3 (see migration.md 5.4/B, D-F, K, 5.5):
 *     - D. clipboard has_text  glfwGetClipboardString probe (set/get are the
 *                              shared glfwSet/GetClipboardString() path in
 *                              guios_common.cpp)
 *     - D. wait cursor ...... no-op (GLFW has no busy cursor shape; see below)
 *     - E. system colors .... fixed sensible values
 *     - F. shell ............ xdg-open / open via fork+exec
 *     - B. resources ........ string table (guios_common.cpp) + the embedded
 *                              runtbar.bmp / license.txt byte arrays
 *                              (guires_data.h)
 *     - K. character encoding TADS charmap layer (charmap.h), routed through
 *                              a small codepage-number -> table-name map
 *                              (below)
 *
 *   D's tick clock is platform-independent (std::chrono) and lives in the
 *   shared guios_common.cpp, not here.
 *
 *   The build gate in CMakeLists.txt (if NOT WIN32 return()) is still
 *   closed, so nothing links this yet - that gate lifts in M4, which is also
 *   the first time any of this can actually be exercised.
 */

#ifdef _WIN32
#error "guios_portable.cpp is the non-Windows backend; Windows builds use guios_w32.cpp"
#endif

#include <cstdio>
#include <cstring>

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <GLFW/glfw3.h>

#include "tadshtml.h"     /* th_malloc / th_free */
#include "guios.h"
#include "htmlres.h"      /* IDS_* string ids */
#include "charmap.h"      /* CCharmapToUni / CCharmapToLocal - item K */
#include "resload.h"      /* CResLoader - finds charmap/<name>.tcm files */


/* ------------------------------------------------------------------------ */
/*
 *   D. Clipboard - has_text only
 *
 *   set/get are the shared glfwSet/GetClipboardString() path in
 *   guios_common.cpp.  GLFW offers no format query, so the only "is there
 *   text" test is a full fetch; unlike Win32 (IsClipboardFormatAvailable),
 *   this backend has nothing cheaper, and can_paste() - which calls this -
 *   runs every frame from the toolbar.  Acceptable for now; a real
 *   non-Windows port can add a lighter probe if it matters.
 */

int os_clipboard_has_text(void)
{
    const char *s = glfwGetClipboardString(NULL);
    return s != NULL && s[0] != '\0';
}


/* ------------------------------------------------------------------------ */
/*
 *   D. Wait cursor
 *
 *   GLFW 3.5 has no busy/hourglass standard cursor (only arrow, I-beam,
 *   crosshair, hand, the resize shapes and not-allowed), and the operations
 *   that want a busy cursor block the frame loop, so there is nothing useful
 *   to set here yet.  guit3 already shows a "Working..." status-line message
 *   alongside every one of these, so the visual cue is not lost.  A real busy
 *   cursor would need a bundled image fed through glfwCreateCursor() - future
 *   work, tracked with item B's other embedded assets.
 */
os_cursor_token_t os_set_wait_cursor(void)
{
    return NULL;
}

void os_restore_cursor(os_cursor_token_t /*prev*/)
{
}


/* ------------------------------------------------------------------------ */
/*
 *   F. Shell integration
 */
int os_open_url(const char *url)
{
#if defined(__APPLE__)
    const char *opener = "open";
#else
    const char *opener = "xdg-open";
#endif

    pid_t pid = fork();
    if (pid < 0)
        return 0;

    if (pid == 0)
    {
        /*
         *   First-generation child.  Detach from our process group/session
         *   so the opener/browser outlives us, then fork *again* (the
         *   classic "double fork" daemonizing idiom) before handing off to
         *   the platform opener in the grandchild.
         *
         *   This second fork is what actually matters here: xdg-open isn't
         *   guaranteed to background itself before its handler exits - on
         *   at least one real system (WSLg, no desktop portal/session
         *   bus), its shell script forks the browser as its own child and
         *   then just sits there instead of exiting, so the browser stays
         *   under xdg-open the whole time it's open. A single-fork parent
         *   blocked in waitpid() on that xdg-open process would then hang
         *   for as long as the browser stays open, not just for the
         *   handoff - which is exactly what happened when this hung guit3
         *   launching a Web UI game under WSL. Forking again means we only
         *   ever wait on the first-generation child below, which exits
         *   immediately regardless of how the opener chain behaves; the
         *   grandchild (and whatever it execs) gets reparented to init.
         */
        setsid();
        pid_t pid2 = fork();
        if (pid2 == 0)
        {
            execlp(opener, opener, url, (char *)NULL);
            _exit(127);
        }
        _exit(pid2 > 0 ? 0 : 1);
    }

    /*
     *   Parent - wait only on the first-generation child, which exits
     *   right away per the above, so this can no longer block on the
     *   opener or the browser it launches.  Its exit status only tells us
     *   whether the second fork() succeeded, not whether the opener itself
     *   later found a working browser - that's the trade-off for never
     *   hanging here.
     */
    int status = 0;
    if (waitpid(pid, &status, 0) != pid)
        return 0;
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}


/* ------------------------------------------------------------------------ */
/*
 *   E. System colors
 *
 *   Off Windows there is no single system palette to read (it varies by
 *   desktop environment and theme), so return fixed values that read well on
 *   a light UI.  Same packed 0x00BBGGRR encoding as a Win32 COLORREF, so the
 *   results still feed COLORREF_to_HTML_color() and GetRValue()/GetGValue()/
 *   GetBValue() unchanged.
 */
static unsigned long pack_bgr(unsigned r, unsigned g, unsigned b)
{
    return (unsigned long)r | ((unsigned long)g << 8) | ((unsigned long)b << 16);
}

unsigned long os_get_sys_color(os_sys_color_t which)
{
    switch (which)
    {
    case OS_SYS_COLOR_HIGHLIGHT:       return pack_bgr(0x00, 0x78, 0xD7); /* blue   */
    case OS_SYS_COLOR_HIGHLIGHT_TEXT:  return pack_bgr(0xFF, 0xFF, 0xFF); /* white  */
    case OS_SYS_COLOR_WINDOW:          return pack_bgr(0xFF, 0xFF, 0xFF); /* white  */
    case OS_SYS_COLOR_WINDOW_TEXT:     return pack_bgr(0x00, 0x00, 0x00); /* black  */
    }
    return 0;
}


/* ------------------------------------------------------------------------ */
/*
 *   B. Bundled resources
 *
 *   Windows pulls these out of the compiled .exe resources (LoadString(),
 *   LoadImage() of IDB_TERP_TOOLBAR, FindResource() of IDX_LICENSE_TEXT).
 *   There's no resource compiler off Windows, so this backend uses a
 *   generated string table (one entry per IDS_* id actually routed
 *   through os_load_string(), text copied verbatim from win32/htmlcmn.rc's
 *   STRINGTABLE) and the embedded runtbar.bmp / license.txt byte arrays in
 *   guires_data.h/.cpp (mechanically generated with `xxd -i`, not
 *   hand-maintained).  All three live in guios_common.cpp, since the
 *   Windows backend falls back to them too.
 */

int os_load_string(int id, char *buf, size_t buflen)
{
    /* no resource compiler here - the shared table is the only source */
    return os_load_builtin_string(id, buf, buflen);
}

unsigned char *os_load_toolbar_rgba(int *width, int *height)
{
    /* no resource compiler here - the embedded copy is the only source */
    return os_load_builtin_toolbar_rgba(width, height);
}

char *os_load_license_text(size_t *len)
{
    return os_load_builtin_license_text(len);
}


/* ------------------------------------------------------------------------ */
/*
 *   K. Character encoding
 *
 *   The Win32 backend converts through a numeric Windows code page
 *   (MultiByteToWideChar/WideCharToMultiByte).  Off Windows there's no such
 *   thing, so this backend routes the same numeric code page through the
 *   TADS charmap layer the VM itself uses to load game character sets
 *   (charmap.h, charmap/<name>.tcm, bundled into charmap/cmaplib.t3r next to
 *   the built executable - see CMakeLists.txt's POST_BUILD step). The
 *   mapping from code page number to charmap table name is just "cp<N>" -
 *   that's the exact naming convention the table files under tads3/charmap/
 *   already use (cp1252.tcm, cp1250.tcm, ...); the one exception is 65001
 *   (CP_UTF8), which needs no table at all.  Loaded mappers are cached
 *   forever (there are only ever a handful of code pages in play in one
 *   process) and a table that fails to load falls back to plain ASCII
 *   rather than failing the conversion outright, so a missing/corrupt
 *   cmaplib.t3r degrades gracefully instead of losing all GUI text.
 *
 *   This mirrors guimain.cpp not yet existing off Windows (M4): there is no
 *   portable "directory the executable lives in" query wired up yet, so the
 *   CResLoader used here has no root directory and therefore searches the
 *   current working directory, same as CResLoader's other bare-constructor
 *   callers (e.g. msgcomp.cpp).  That's fine for the common case of running
 *   guit3 from its own install/build directory; if that proves too fragile
 *   once there's a real M4 Linux build to test against, give this its own
 *   exe-relative CResLoader the way t3main.cpp's MyHostIfc does.
 */

namespace {

void codepage_table_name(unsigned int codepage, char *buf, size_t buflen)
{
    if (codepage == 65001 /* CP_UTF8 */)
        snprintf(buf, buflen, "utf-8");
    else if (codepage == 0 /* CP_ACP */)
        snprintf(buf, buflen, "cp1252");   /* matches htmlgui.cpp's own default */
    else
        snprintf(buf, buflen, "cp%u", codepage);
}

CResLoader *cmap_res_loader()
{
    static CResLoader *loader = new CResLoader();
    return loader;
}

/* small linear-scan caches - only ever a handful of code pages are live */
struct to_uni_cache_entry_t { unsigned int codepage; CCharmapToUni *cmap; };
struct to_local_cache_entry_t { unsigned int codepage; CCharmapToLocal *cmap; };

CCharmapToUni *get_to_uni(unsigned int codepage)
{
    static to_uni_cache_entry_t cache[16];
    static int cache_cnt = 0;

    for (int i = 0 ; i < cache_cnt ; ++i)
        if (cache[i].codepage == codepage)
            return cache[i].cmap;

    char table_name[32];
    codepage_table_name(codepage, table_name, sizeof(table_name));

    CCharmapToUni *cmap = CCharmapToUni::load(cmap_res_loader(), table_name);
    if (cmap == 0)
        cmap = new CCharmapToUniASCII();

    if (cache_cnt < (int)(sizeof(cache) / sizeof(cache[0])))
        cache[cache_cnt++] = { codepage, cmap };

    return cmap;
}

CCharmapToLocal *get_to_local(unsigned int codepage)
{
    static to_local_cache_entry_t cache[16];
    static int cache_cnt = 0;

    for (int i = 0 ; i < cache_cnt ; ++i)
        if (cache[i].codepage == codepage)
            return cache[i].cmap;

    char table_name[32];
    codepage_table_name(codepage, table_name, sizeof(table_name));

    CCharmapToLocal *cmap = CCharmapToLocal::load(cmap_res_loader(), table_name);
    if (cmap == 0)
        cmap = new CCharmapToLocalASCII();

    if (cache_cnt < (int)(sizeof(cache) / sizeof(cache[0])))
        cache[cache_cnt++] = { codepage, cmap };

    return cmap;
}

} // namespace

char *os_local_to_utf8(unsigned int codepage,
                       const char *src, size_t srclen, size_t *out_len)
{
    CCharmapToUni *cmap = get_to_uni(codepage);

    size_t needed = cmap->map_str(0, 0, src, srclen);
    char *buf = (char *)th_malloc(needed + 1);
    cmap->map_str(buf, needed, src, srclen);
    buf[needed] = '\0';

    if (out_len != 0)
        *out_len = needed;
    return buf;
}

os_utf16_t *os_local_to_utf16(unsigned int codepage,
                              const char *src, size_t srclen, size_t *out_cnt)
{
    CCharmapToUni *cmap = get_to_uni(codepage);

    /* one local character can never expand to more than one code unit, so
       srclen units is always enough room */
    os_utf16_t *out = (os_utf16_t *)
        th_malloc((srclen > 0 ? srclen : 1) * sizeof(os_utf16_t));

    size_t n = 0;
    const char *p = src;
    size_t len = srclen;
    wchar_t ch;
    while (len > 0 && cmap->mapchar(ch, p, len))
        out[n++] = (os_utf16_t)ch;

    if (out_cnt != 0)
        *out_cnt = n;
    return out;
}

char *os_utf8_to_local(unsigned int codepage, const char *utf8, size_t *out_len)
{
    CCharmapToLocal *cmap = get_to_local(codepage);

    utf8_ptr src((char *)utf8);
    size_t needed = cmap->map_utf8z(0, 0, src);
    char *buf = (char *)th_malloc(needed + 1);
    cmap->map_utf8z(buf, needed + 1, src);
    buf[needed] = '\0';

    if (out_len != 0)
        *out_len = needed;
    return buf;
}


/* ------------------------------------------------------------------------ */
/*
 *   L. Keyboard - current-layout char<->key queries.
 *
 *   GLFW's named GLFW_KEY_* constants for the printable range are
 *   deliberately identical to the ASCII/US-layout character codes they
 *   produce unshifted (GLFW_KEY_A==65=='A', GLFW_KEY_COMMA==44==',', ...),
 *   which is what makes a table-free os_key_to_char() possible; shifted
 *   punctuation/digits still need an explicit table since the shifted
 *   character isn't the key's own GLFW_KEY_* value. There is no portable
 *   "ask the OS for the live keyboard layout" API (see guios.h's own note),
 *   so like guios_w32.cpp's VkKeyScan()/MapVirtualKey() calls, this assumes
 *   a US layout.
 */
int os_key_to_char(os_key_t key)
{
    if (key >= GLFW_KEY_SPACE && key <= GLFW_KEY_GRAVE_ACCENT)
        return key;
    return 0;
}

os_key_t os_char_to_key(int ch, int *shift_out)
{
    *shift_out = 0;

    if (ch >= 'a' && ch <= 'z')
        return GLFW_KEY_A + (ch - 'a');
    if (ch >= 'A' && ch <= 'Z')
    {
        *shift_out = OS_KEY_SHIFT;
        return ch;
    }
    if (ch >= '0' && ch <= '9')
        return GLFW_KEY_0 + (ch - '0');

    switch (ch)
    {
    case ' ':  return GLFW_KEY_SPACE;
    case '`':  return GLFW_KEY_GRAVE_ACCENT;
    case '-':  return GLFW_KEY_MINUS;
    case '=':  return GLFW_KEY_EQUAL;
    case '[':  return GLFW_KEY_LEFT_BRACKET;
    case ']':  return GLFW_KEY_RIGHT_BRACKET;
    case '\\': return GLFW_KEY_BACKSLASH;
    case ';':  return GLFW_KEY_SEMICOLON;
    case '\'': return GLFW_KEY_APOSTROPHE;
    case ',':  return GLFW_KEY_COMMA;
    case '.':  return GLFW_KEY_PERIOD;
    case '/':  return GLFW_KEY_SLASH;
    }

    /* shifted digit row: Shift+1..Shift+0 -> !@#$%^&*() */
    static const char shifted_digits[] = "!@#$%^&*()";
    const char *sd = strchr(shifted_digits, ch);
    if (ch != 0 && sd != 0)
    {
        *shift_out = OS_KEY_SHIFT;
        return GLFW_KEY_0 + (int)((sd - shifted_digits + 1) % 10);
    }

    /* other shifted punctuation */
    static const struct { char ch; int key; } shifted[] =
    {
        { '~', GLFW_KEY_GRAVE_ACCENT }, { '_', GLFW_KEY_MINUS },
        { '+', GLFW_KEY_EQUAL },        { '{', GLFW_KEY_LEFT_BRACKET },
        { '}', GLFW_KEY_RIGHT_BRACKET },{ '|', GLFW_KEY_BACKSLASH },
        { ':', GLFW_KEY_SEMICOLON },    { '"', GLFW_KEY_APOSTROPHE },
        { '<', GLFW_KEY_COMMA },        { '>', GLFW_KEY_PERIOD },
        { '?', GLFW_KEY_SLASH },
    };
    for (size_t i = 0 ; i < sizeof(shifted)/sizeof(shifted[0]) ; ++i)
    {
        if (shifted[i].ch == ch)
        {
            *shift_out = OS_KEY_SHIFT;
            return shifted[i].key;
        }
    }

    return 0;
}

/*
 *   Portable stand-in for the IDR_ACCEL_WIN/IDR_ACCEL_EMACS ACCELERATORS
 *   resources (win32/htmlcmn.rc) - hand-transcribed, so keep both in sync
 *   with the .rc if the bindings ever change there.
 */
static const os_accel_entry_t accel_win[] =
{
    { GLFW_KEY_A, OS_KEY_CTRL, ID_EDIT_SELECTALL },
    { GLFW_KEY_C, OS_KEY_CTRL, ID_EDIT_COPY },
    { GLFW_KEY_INSERT, OS_KEY_CTRL, ID_EDIT_COPY },
    { GLFW_KEY_F, OS_KEY_CTRL, ID_EDIT_FIND },
    { GLFW_KEY_X, OS_KEY_CTRL, ID_EDIT_CUT },
    { GLFW_KEY_DELETE, OS_KEY_SHIFT, ID_EDIT_CUT },
    { GLFW_KEY_V, OS_KEY_CTRL, ID_EDIT_PASTE },
    { GLFW_KEY_INSERT, OS_KEY_SHIFT, ID_EDIT_PASTE },
    { GLFW_KEY_Z, OS_KEY_CTRL, ID_EDIT_UNDO },
    { GLFW_KEY_Q, OS_KEY_CTRL, ID_FILE_QUIT },
    { GLFW_KEY_S, OS_KEY_CTRL, ID_FILE_SAVEGAME },
    { GLFW_KEY_R, OS_KEY_CTRL, ID_FILE_RESTOREGAME },
    { GLFW_KEY_O, OS_KEY_CTRL, ID_FILE_LOADGAME },
    { GLFW_KEY_PERIOD, OS_KEY_ALT, ID_GO_NEXT },
    { GLFW_KEY_COMMA, OS_KEY_ALT, ID_GO_PREVIOUS },
    { GLFW_KEY_F1, 0, ID_HELP_COMMAND },
    { GLFW_KEY_F3, 0, ID_EDIT_FINDNEXT },
};

static const os_accel_entry_t accel_emacs[] =
{
    { GLFW_KEY_A, OS_KEY_CTRL, ID_EDIT_SELECTALL },
    { GLFW_KEY_C, OS_KEY_CTRL, ID_EDIT_COPY },
    { GLFW_KEY_INSERT, OS_KEY_CTRL, ID_EDIT_COPY },
    { GLFW_KEY_F, OS_KEY_CTRL, ID_EDIT_FIND },
    { GLFW_KEY_X, OS_KEY_CTRL, ID_EDIT_CUT },
    { GLFW_KEY_DELETE, OS_KEY_SHIFT, ID_EDIT_CUT },
    { GLFW_KEY_Y, OS_KEY_CTRL, ID_EDIT_PASTE },
    { GLFW_KEY_INSERT, OS_KEY_SHIFT, ID_EDIT_PASTE },
    { GLFW_KEY_Z, OS_KEY_CTRL, ID_EDIT_UNDO },
    { GLFW_KEY_Q, OS_KEY_CTRL, ID_FILE_QUIT },
    { GLFW_KEY_S, OS_KEY_CTRL, ID_FILE_SAVEGAME },
    { GLFW_KEY_R, OS_KEY_CTRL, ID_FILE_RESTOREGAME },
    { GLFW_KEY_O, OS_KEY_CTRL, ID_FILE_LOADGAME },
    { GLFW_KEY_PERIOD, OS_KEY_ALT, ID_GO_NEXT },
    { GLFW_KEY_COMMA, OS_KEY_ALT, ID_GO_PREVIOUS },
    { GLFW_KEY_F1, 0, ID_HELP_COMMAND },
    { GLFW_KEY_F3, 0, ID_EDIT_FINDNEXT },
};

int os_load_accel_table(int accel_id, os_accel_entry_t *entries,
                        int max_entries)
{
    const os_accel_entry_t *src;
    int src_cnt;

    if (accel_id == IDR_ACCEL_WIN)
        src = accel_win, src_cnt = sizeof(accel_win)/sizeof(accel_win[0]);
    else if (accel_id == IDR_ACCEL_EMACS)
        src = accel_emacs, src_cnt = sizeof(accel_emacs)/sizeof(accel_emacs[0]);
    else
        return 0;

    int n = src_cnt < max_entries ? src_cnt : max_entries;
    memcpy(entries, src, n * sizeof(entries[0]));
    return n;
}


/* ------------------------------------------------------------------------ */
/*
 *   M. Debug console - a console window isn't needed off Windows since
 *   stdout already goes somewhere visible (the terminal guit3 was launched
 *   from), so both hooks are no-ops.
 */
void os_init_debug_console(void) { }
void os_close_debug_console(void) { }
