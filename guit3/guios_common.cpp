/*
 *   guios_common.cpp - platform-independent backend for the guit3 OS-service
 *   hooks (guios.h)
 *
 *   Most guios.h hooks have a genuinely different implementation per platform
 *   and live in guios_w32.cpp / guios_portable.cpp.  A few are the same C++
 *   everywhere; those live here, and this file is compiled into every build
 *   alongside exactly one of the per-platform backends (see
 *   htmltads/imgui/CMakeLists.txt).
 */

#include <chrono>
#include <cstring>

#include <GLFW/glfw3.h>

#include "tadshtml.h"     /* th_malloc / th_free */
#include "htmlres.h"     /* IDS_* */
#include "guires_data.h" /* embedded runtbar.bmp / license.txt bytes */
#include "guios.h"


/* ------------------------------------------------------------------------ */
/*
 *   D. Clipboard (plain text, UTF-8)
 *
 *   GLFW moves UTF-8 bytes to/from the system clipboard on every platform
 *   (Win32: CF_UNICODETEXT), so set/get are the same code everywhere and
 *   live here rather than in the per-platform backends.  The window argument
 *   has been deprecated-and-ignored since GLFW 3.0, so NULL is fine.
 *
 *   The engine's text is local-codepage; the copy/paste call sites in
 *   htmlgui.cpp convert with os_local_to_utf8() / os_utf8_to_local() (item K)
 *   on the way through.  CR/LF normalization stays the caller's job, exactly
 *   as with the old CF_TEXT path.
 *
 *   os_clipboard_has_text() is deliberately NOT here - see guios.h.
 */

int os_clipboard_set_text(const char *text)
{
    glfwSetClipboardString(NULL, text);
    return 1;
}

char *os_clipboard_get_text(void)
{
    const char *s = glfwGetClipboardString(NULL);
    if (s == NULL)
        return NULL;

    /* GLFW owns 's' only until the next clipboard call, so copy it now */
    size_t len = strlen(s) + 1;
    char *result = (char *)th_malloc(len);
    if (result != NULL)
        memcpy(result, s, len);
    return result;
}


/* ------------------------------------------------------------------------ */
/*
 *   D. Millisecond tick clock
 *
 *   std::chrono::steady_clock is monotonic and high-resolution on every
 *   target we care about (MSVC backs it with QueryPerformanceCounter), which
 *   makes it a strict upgrade over the old Win32 GetTickCount() - finer
 *   granularity, and callers only ever diff two readings anyway.  Measured
 *   from the first call; as an unsigned long the count wraps after ~49 days
 *   of process uptime where long is 32-bit (Windows) and effectively never
 *   where it is 64-bit.
 */
unsigned long os_get_tick_ms(void)
{
    using namespace std::chrono;
    static const steady_clock::time_point start = steady_clock::now();
    return (unsigned long)
        duration_cast<milliseconds>(steady_clock::now() - start).count();
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
 *   top-down 32bpp RGBA buffer (th_malloc()'d) with *width/*height filled
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
