/*
 *   guios_common.cpp - platform-independent backend for the guit3 OS-service
 *   hooks (guios.h)
 *
 *   Most guios.h hooks have a genuinely different implementation per platform
 *   and live in guios_w32.cpp / guios_portable.cpp.  The ones that are the
 *   same C++ everywhere, mostly because the engine's platform services
 *   (CGE::Platform) do the per-platform work, live here: the clipboard, the
 *   tick clock, the wait cursor and opening URLs.  This file is compiled
 *   into every build alongside exactly one of the per-platform backends
 *   (see guit3/CMakeLists.txt).
 */

#include <cstdint>
#include <cstring>
#include <string>

#include <platform/services.h>

#include "tadshtml.h"     /* th_malloc / th_free */
#include "htmlres.h"     /* IDS_* */
#include "guires_data.h" /* embedded runtbar.bmp / license.txt bytes */
#include "guios.h"


/* ------------------------------------------------------------------------ */
/*
 *   D. Clipboard (plain text, UTF-8)
 *
 *   The engine's platform services move UTF-8 text to/from the system
 *   clipboard on every platform (Windows: CF_UNICODETEXT; Linux: the X11
 *   CLIPBOARD selection; the web: the text copied or pasted last in the
 *   page, kept current by the browser's paste event - see
 *   CGE::Platform in the engine's platform/services.h).
 *
 *   The TADS engine's text is local-codepage; the copy/paste call sites in
 *   htmlgui.cpp convert with os_local_to_utf8() / os_utf8_to_local() (item
 *   K) on the way through.  CR/LF normalization stays the caller's job,
 *   exactly as with the old CF_TEXT path.
 */

int os_clipboard_set_text(const char *text)
{
    return CGE::Platform::setClipboardText(text) ? 1 : 0;
}

char *os_clipboard_get_text(void)
{
    std::string s = CGE::Platform::getClipboardText();
    if (s.empty())
        return NULL;

    char *result = (char *)th_malloc(s.size() + 1);
    if (result != NULL)
        memcpy(result, s.c_str(), s.size() + 1);
    return result;
}

int os_clipboard_has_text(void)
{
    return CGE::Platform::hasClipboardText() ? 1 : 0;
}


/* ------------------------------------------------------------------------ */
/*
 *   L. The ImGui key of a canonical key code (guikeys.h)
 */
ImGuiKey gui_key_to_imgui(int key)
{
    if (key >= GUI_KEY_A && key <= GUI_KEY_A + 25)
        return (ImGuiKey)(ImGuiKey_A + (key - GUI_KEY_A));
    if (key >= GUI_KEY_0 && key <= GUI_KEY_9)
        return (ImGuiKey)(ImGuiKey_0 + (key - GUI_KEY_0));
    if (key >= GUI_KEY_F1 && key <= GUI_KEY_F24)
        return (ImGuiKey)(ImGuiKey_F1 + (key - GUI_KEY_F1));
    if (key >= GUI_KEY_KP_0 && key <= GUI_KEY_KP_9)
        return (ImGuiKey)(ImGuiKey_Keypad0 + (key - GUI_KEY_KP_0));

    switch (key)
    {
    case GUI_KEY_SPACE:         return ImGuiKey_Space;
    case GUI_KEY_APOSTROPHE:    return ImGuiKey_Apostrophe;
    case GUI_KEY_COMMA:         return ImGuiKey_Comma;
    case GUI_KEY_MINUS:         return ImGuiKey_Minus;
    case GUI_KEY_PERIOD:        return ImGuiKey_Period;
    case GUI_KEY_SLASH:         return ImGuiKey_Slash;
    case GUI_KEY_SEMICOLON:     return ImGuiKey_Semicolon;
    case GUI_KEY_EQUAL:         return ImGuiKey_Equal;
    case GUI_KEY_LEFT_BRACKET:  return ImGuiKey_LeftBracket;
    case GUI_KEY_BACKSLASH:     return ImGuiKey_Backslash;
    case GUI_KEY_RIGHT_BRACKET: return ImGuiKey_RightBracket;
    case GUI_KEY_GRAVE_ACCENT:  return ImGuiKey_GraveAccent;
    case GUI_KEY_ESCAPE:        return ImGuiKey_Escape;
    case GUI_KEY_ENTER:         return ImGuiKey_Enter;
    case GUI_KEY_TAB:           return ImGuiKey_Tab;
    case GUI_KEY_BACKSPACE:     return ImGuiKey_Backspace;
    case GUI_KEY_INSERT:        return ImGuiKey_Insert;
    case GUI_KEY_DELETE:        return ImGuiKey_Delete;
    case GUI_KEY_RIGHT:         return ImGuiKey_RightArrow;
    case GUI_KEY_LEFT:          return ImGuiKey_LeftArrow;
    case GUI_KEY_DOWN:          return ImGuiKey_DownArrow;
    case GUI_KEY_UP:            return ImGuiKey_UpArrow;
    case GUI_KEY_PAGE_UP:       return ImGuiKey_PageUp;
    case GUI_KEY_PAGE_DOWN:     return ImGuiKey_PageDown;
    case GUI_KEY_HOME:          return ImGuiKey_Home;
    case GUI_KEY_END:           return ImGuiKey_End;
    case GUI_KEY_PRINT_SCREEN:  return ImGuiKey_PrintScreen;
    case GUI_KEY_PAUSE:         return ImGuiKey_Pause;
    case GUI_KEY_KP_DECIMAL:    return ImGuiKey_KeypadDecimal;
    case GUI_KEY_KP_DIVIDE:     return ImGuiKey_KeypadDivide;
    case GUI_KEY_KP_MULTIPLY:   return ImGuiKey_KeypadMultiply;
    case GUI_KEY_KP_SUBTRACT:   return ImGuiKey_KeypadSubtract;
    case GUI_KEY_KP_ADD:        return ImGuiKey_KeypadAdd;
    default:                    return ImGuiKey_None;
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   D. Millisecond tick clock
 *
 *   The engine's steady clock (CGE::Platform::getTickMs()), measured from
 *   its first call.  As an unsigned long the count wraps after ~49 days of
 *   process uptime where long is 32-bit (Windows) and effectively never
 *   where it is 64-bit; callers only ever diff two readings.
 */
unsigned long os_get_tick_ms(void)
{
    return (unsigned long)CGE::Platform::getTickMs();
}


/* ------------------------------------------------------------------------ */
/*
 *   D. Wait cursor
 *
 *   CGE::Platform::setCursor() takes effect at once, also while the
 *   operation blocks the frame loop.  The token is the previous
 *   CGE::Platform::Cursor.  The engine's ImGui backend sets ImGui's cursor
 *   again in the next frame anyway.
 */
os_cursor_token_t os_set_wait_cursor(void)
{
    CGE::Platform::Cursor prev = CGE::Platform::getCursor();
    CGE::Platform::setCursor(CGE::Platform::CURSOR_WAIT);
    return (os_cursor_token_t)(intptr_t)prev;
}

void os_restore_cursor(os_cursor_token_t prev)
{
    CGE::Platform::setCursor((CGE::Platform::Cursor)(intptr_t)prev);
}


/* ------------------------------------------------------------------------ */
/*
 *   F. Shell integration
 *
 *   CGE::Platform::openUrl(): ShellExecute "open" on Windows, xdg-open
 *   (detached) on Linux, a new browser tab on the web.  It takes UTF-8; the
 *   URLs come from the game's text in the local code page, so they're
 *   converted first (ASCII, which nearly every URL is, stays the same).
 */
int os_open_url(const char *url)
{
    char *utf8 = os_local_to_utf8(0 /* CP_ACP */, url, strlen(url), NULL);
    if (utf8 == NULL)
        return 0;

    int ok = CGE::Platform::openUrl(utf8) ? 1 : 0;
    th_free(utf8);
    return ok;
}

/* ------------------------------------------------------------------------ */
/*
 *   B. Built-in UI string table
 *
 *   One entry per IDS_* id actually routed through os_load_string(), text
 *   copied verbatim from win32/htmlcmn.rc's STRINGTABLE.  This is the only
 *   source off Windows (no resource compiler), and the Windows backend falls
 *   back to it as well, since this repo doesn't carry the .rc files either
 *   (see the NOTE in CMakeLists.txt) - LoadString() then finds nothing and
 *   would hand ImGui empty labels.
 */

namespace {

struct string_table_entry_t { int id; const char *text; };
const string_table_entry_t string_table[] =
{
    { IDS_MORE_PROMPT,           "  *** More ***  " },
    { IDS_MORE_STATUS_MSG,       "*** MORE *** [press the space bar to continue]" },
    { IDS_WORKING_MSG,           "Working..." },
    { IDS_PRESS_A_KEY_MSG,       "Please press a key..." },
    { IDS_EXIT_PAUSE_MSG,        "Press any key to exit." },
    { IDS_NO_GAME_MSG,           "(No game loaded.)" },
    { IDS_GAME_OVER_MSG,         "(The game has ended.)" },
    { IDS_FIND_NO_MORE,          "No more matches found" },
    { IDS_CANNOT_OPEN_HREF,      "Unable to start browser. You must have a web browser installed to show this link." },
    { IDS_LINK_PREF_CHANGE,      "Note: the Game Chest page always shows links, so your change to the 'Show Links' setting won't affect the current page. The change will take effect when you play a game using this theme." },
    { IDS_ABOUT_GAME_WIN_TITLE,  "About This Game" },
    { IDS_REALLY_NEW_GAME_MSG,   "Starting a new game will quit the current game without saving. Are you sure you want to proceed?" },
    { IDS_REALLY_QUIT_MSG,       "You are about to quit the game without saving. Do you really want to quit?" },
    { IDS_MANAGE_PROFILES,       "&Add/Delete Themes..." },
    { IDS_SET_DEF_PROFILE,       "Set \"%s\" as &Default Theme" },
    { IDS_CUSTOMIZE_THEME,       "&Customize \"%s\" Theme..." },
    { IDS_THEMES_DROPDOWN,       "Customize \"%s\" Theme" },
    { IDS_REALLY_GO_GC_MSG,      "This will quit the current game without saving your position - any work that you have done since you last saved will be lost. Do you really want to do this?" },
    { IDS_CHOOSE_NEW_GAME,       "Choose a game to load" },
    { IDS_ABOUTBOX_1,            "<font color=white face=Arial size=-1><b>Release " },
    { IDS_ABOUTBOX_2,            "</b></font><br><br><tab align=right><font face='Arial' size=-1><b><a forced href='http://www.tads.org/'>www.tads.org</a> &nbsp;&nbsp; <a forced href='credits'>Credits</a> &nbsp;&nbsp; <a forced href='license'>License</a> &nbsp;&nbsp; <a forced href='close'>Close</a></b></font>" },
    { IDS_CHOOSE_GAME,           "Select a TADS Game" },
    { IDS_THEMEDESC_MULTIMEDIA,  "The basic Windows look and feel." },
    { IDS_THEMEDESC_PLAIN_TEXT,  "A retro look recalling the classic text adventures of the 80's." },
    { IDS_THEMEDESC_WEB_STYLE,   "A clean, modern look based on popular Web page styles." },
};
const int string_table_cnt = sizeof(string_table) / sizeof(string_table[0]);

} // namespace

int os_load_builtin_string(int id, char *buf, size_t buflen)
{
    for (int i = 0 ; i < string_table_cnt ; ++i)
    {
        if (string_table[i].id == id)
        {
            size_t len = strlen(string_table[i].text);
            if (len >= buflen)
                len = buflen > 0 ? buflen - 1 : 0;
            memcpy(buf, string_table[i].text, len);
            if (buflen > 0)
                buf[len] = '\0';
            return (int)len;
        }
    }

    if (buflen > 0)
        buf[0] = '\0';
    return 0;
}


/* ------------------------------------------------------------------------ */
/*
 *   B. Embedded toolbar bitmap and license text
 *
 *   The byte arrays from guires_data.h/.cpp, decoded into the same shapes
 *   os_load_toolbar_rgba() / os_load_license_text() return.  The only
 *   source off Windows; the Windows backend falls back to them when the
 *   .exe carries no IDB_TERP_TOOLBAR / IDX_LICENSE_TEXT resources (see
 *   the NOTE in CMakeLists.txt).
 */

/*
 *   Minimal BMP reader for runtbar.bmp's exact format: an uncompressed,
 *   palette-indexed (1/4/8 bpp) Windows DIB, which is all a toolbar icon
 *   strip like this has ever needed.  Fields are read a byte at a time
 *   rather than through a packed struct, since BMP's on-disk layout doesn't
 *   match any C++ struct's natural alignment.  Returns a newly allocated
 *   top-down 32bpp RGBA buffer (th_malloc()'d) with *width / *height filled
 *   in, or null if the data isn't a BMP in one of these formats.
 */
namespace {

unsigned int le16(const unsigned char *p) { return p[0] | (p[1] << 8); }
unsigned int le32(const unsigned char *p)
{
    return p[0] | (p[1] << 8) | (p[2] << 16) | ((unsigned int)p[3] << 24);
}

unsigned char *decode_indexed_bmp(const unsigned char *data, size_t size,
                                  int *width, int *height)
{
    if (size < 54 || data[0] != 'B' || data[1] != 'M')
        return 0;

    unsigned int bits_offset = le32(data + 10);
    unsigned int hdr_size = le32(data + 14);
    int w = (int)le32(data + 18);
    int h = (int)le32(data + 22);
    unsigned int bitcount = le16(data + 28);
    unsigned int compression = le32(data + 30);
    unsigned int colors_used = le32(data + 46);

    if (compression != 0 /* BI_RGB */
        || (bitcount != 1 && bitcount != 4 && bitcount != 8)
        || w <= 0 || h == 0)
        return 0;

    int top_down = (h < 0);
    if (top_down)
        h = -h;

    if (colors_used == 0)
        colors_used = 1u << bitcount;
    const unsigned char *palette = data + 14 + hdr_size;
    if (palette + colors_used * 4 > data + size)
        return 0;

    unsigned int row_bytes = ((w * bitcount + 31) / 32) * 4;
    if (bits_offset + (size_t)row_bytes * h > size)
        return 0;

    unsigned char *pixels = (unsigned char *)th_malloc((size_t)w * h * 4);
    for (int y = 0 ; y < h ; ++y)
    {
        /* BMP rows are bottom-up unless the height field was negative */
        int src_row = top_down ? y : (h - 1 - y);
        const unsigned char *row = data + bits_offset
            + (size_t)src_row * row_bytes;
        unsigned char *out = pixels + (size_t)y * w * 4;

        for (int x = 0 ; x < w ; ++x)
        {
            unsigned int idx;
            if (bitcount == 8)
                idx = row[x];
            else if (bitcount == 4)
                idx = (x & 1) ? (row[x / 2] & 0x0f) : (row[x / 2] >> 4);
            else /* bitcount == 1 */
                idx = (row[x / 8] >> (7 - (x % 8))) & 1;

            const unsigned char *bgr = palette + idx * 4;
            out[x*4 + 0] = bgr[2];   /* R */
            out[x*4 + 1] = bgr[1];   /* G */
            out[x*4 + 2] = bgr[0];   /* B */
            out[x*4 + 3] = 0xFF;
        }
    }

    *width = w;
    *height = h;
    return pixels;
}

} // namespace

unsigned char *os_load_builtin_toolbar_rgba(int *width, int *height)
{
    unsigned char *pixels = decode_indexed_bmp(
        g_runtbar_bmp_data, g_runtbar_bmp_size, width, height);
    if (pixels == 0)
        return 0;

    /* same color-key -> alpha conversion as the Win32 LoadImage() path: the top-left
       pixel's color is the mask color, turned into a zero alpha channel */
    unsigned char mask_r = pixels[0], mask_g = pixels[1], mask_b = pixels[2];
    int npix = (*width) * (*height);
    for (int i = 0 ; i < npix ; ++i)
    {
        unsigned char *p = pixels + i*4;
        p[3] = (p[0] == mask_r && p[1] == mask_g && p[2] == mask_b) ? 0 : 255;
    }

    return pixels;
}

char *os_load_builtin_license_text(size_t *len)
{
    char *result = (char *)th_malloc(g_license_txt_size);
    memcpy(result, g_license_txt_data, g_license_txt_size);
    *len = g_license_txt_size;
    return result;
}
