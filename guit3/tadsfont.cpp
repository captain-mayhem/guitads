#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/html/win32/tadsfont.cpp,v 1.3 1999/07/11 00:46:48 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1997 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tadsfont.cpp - TADS font implementation for Win 32
Function
  
Notes
  
Modified
  09/20/97 MJRoberts  - Creation
*/

#include "tadsplat.h"
#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include <misc/freetype/imgui_freetype.h>
#include <string>
#include <platform/systemfonts.h>

#include "guiengine.h"
#include "guios.h"
#include "tadshtml.h"     /* th_free */

#ifndef TADSFONT_H
#include "tadsfont.h"
#endif

/*
 *   Create a TADS font object for a given logical font description
 */
CTadsFont::CTadsFont(const CTadsLOGFONT *logfont)
{
    /* store a canonical copy of the LOGFONT for later comparison */
    copy_canonical_logfont(&logfont_, logfont);

    /*
     *   Create the system font handle.  This is still needed on Windows for
     *   the GDI text-metrics path in CHtmlSysWin_win32::measure_text()
     *   (GetTextMetrics/GetTextExtentPoint32); everything ImGui draws goes
     *   through m_font below.
     */
    handle_ = CreateFontIndirect(&logfont->lf);
    m_font = nullptr;

    /*
     *   Resolve the logical font to the raw bytes of an actual
     *   TrueType/OpenType file so FreeType has something to rasterize.
     *   There is no FreeType (or other OS-agnostic) way to turn a font
     *   *name* into font-file data - that matching is inherently OS
     *   integration - so it lives behind the os_font_data_for_name()
     *   platform hook (declared in tadsfont.h, on the engine's system font
     *   lookup below).  A font collection (.ttc) comes back whole, with the
     *   face to use.
     */
    size_t data_size = 0;
    int face_index = 0;
    unsigned char *buffer = os_font_data_for_name(
        logfont->lf.lfFaceName, (int)logfont->lf.lfWeight,
        logfont->lf.lfItalic, logfont->lf.lfCharSet, &data_size,
        &face_index);
    if (buffer != nullptr) {
        ImFontConfig font_cfg;
        strncpy(font_cfg.Name, logfont->lf.lfFaceName, 40);
        font_cfg.FontNo = face_index;
        font_cfg.FontLoaderFlags = ImGuiFreeTypeLoaderFlags_Bitmap;
        m_font = ImGui::GetIO().Fonts->AddFontFromMemoryTTF(
            buffer, (int)data_size, -logfont->lf.lfHeight, &font_cfg, 0);
    }
    /*
     *   else: this face has no scalable outline data for the hook to
     *   extract - notably the "System" pseudo-font (a legacy bitmap/raster
     *   font, not a real TrueType/OpenType face), which is exactly what
     *   CreateFontIndirect() selects for the literal face name "System"
     *   (e.g. from the Customize Theme dialog's font dropdown).  Leave
     *   m_font null - select()/get_baked()/push_imgui_font() all fall back
     *   to the atlas default font rather than pushing null (which
     *   ImGui::PushFont() treats as "keep the current font", asserting if
     *   that is itself null).
     */
}

CTadsFont::~CTadsFont()
{
    /* we're done with the font */
    if (handle_ != 0)
    {
        DeleteObject(handle_);
        handle_ = 0;
    }
    if (m_font != nullptr && ImGui::GetCurrentContext() != nullptr) {
        ImGuiIO& io = ImGui::GetIO();
        io.Fonts->RemoveFont(m_font);
    }
}

/*
 *   select the font into a device context, returning the previously
 *   selected font's handle
 */
HGDIOBJ CTadsFont::select(HDC dc)
{
    /* push the ImGui font, then select the GDI font into the DC */
    push_imgui_font();
    return SelectObject(dc, handle_);
}

/*
 *   Push this font onto the ImGui font stack, with no DC / GDI involved.
 *
 *   If this font has no loaded ImFont (its underlying system font - e.g.
 *   the "System" pseudo-font - had no scalable outline data for FreeType to
 *   use; see the constructor), fall back to the atlas's default font rather
 *   than pushing null.  ImGui::PushFont(nullptr) doesn't mean "use the
 *   default font" - it means "keep whatever font is currently on the
 *   context's font stack," which can itself still be null this early (e.g.
 *   during the very first HTML layout pass at startup, before any
 *   ImGui::NewFrame() has pushed anything), and it asserts rather than
 *   tolerating that.  io.Fonts->Fonts[0] is always valid once past
 *   htmlgui.cpp's do_create(), which calls AddFontDefault() immediately
 *   after creating the ImGui context, long before any CTadsFont exists.
 */
void CTadsFont::push_imgui_font()
{
    ImGui::PushFont(m_font != nullptr ? m_font : ImGui::GetIO().Fonts->Fonts[0]);
}

/*
 *   restore a font that was in effect before selecting this font 
 */
void CTadsFont::unselect(HDC dc, HGDIOBJ oldfont)
{
    SelectObject(dc, oldfont);
}


/*
 *   Get the screen's logical DPI.  See the declaration in tadsfont.h for
 *   why 96 is the right baseline here: it's the same convention
 *   CTadsSyswin::syswin_create_system_window() already uses (the engine's
 *   window reports a content scale of 1.0 at "100%" scaling, which GDI
 *   would have reported as 96 DPI).
 */
float CTadsFont::get_screen_dpi()
{
    return 96.0f * get_dpi_scale();
}

/*
 *   The window's content scale factor.  See tadsfont.h.
 */
float CTadsFont::get_dpi_scale()
{
    return gui_content_scale();
}

/*
 *   Calculate a setting for LOGFONT.lfHeight, given a point size
 */
long CTadsFont::calc_lfHeight(int pointsize)
{
    /* one inch is 72 points - calculate how many pixels that is */
    long sz = (long)(pointsize * get_screen_dpi()) / 72;

    /* return a negative value to tell Windows to use character size */
    return -sz;
}

/*
 *   Calculate a point size for a given pixel height
 */
int CTadsFont::calc_pointsize(int pix_height)
{
    /*
     *   there are 72 points in an inch, so calculate our pixel height in
     *   inches and multiply the result by 72
     */
    return (int)(((long)pix_height * 72L) / (long)get_screen_dpi());
}

/*
 *   check if we match a logical font 
 */
int CTadsFont::matches(const CTadsLOGFONT *lf)
{
    CTadsLOGFONT canon_lf;

    /*
     *   make a private copy in canonical form, with the lfFaceName array
     *   set to nulls after the terminator 
     */
    copy_canonical_logfont(&canon_lf, lf);
    
    /* if we match the LOGFONT structure exactly, it's a match */
    return memcmp(&canon_lf, &logfont_, sizeof(logfont_)) == 0;
}

/*
 *   Determine if a font is present on the system.  The actual system
 *   font-enumeration work is OS-specific - see the os_font_family_is_present()
 *   declaration in tadsfont.h for why - so this just forwards to the hook.
 */
int CTadsFont::font_is_present(const char *fontname, size_t len)
{
    return os_font_family_is_present(fontname, len);
}


/* ------------------------------------------------------------------------ */
/*
 *   The font platform hooks declared in tadsfont.h, on the engine's system
 *   font lookup (CGE::SystemFonts: GDI on Windows, fontconfig on Linux, the
 *   bundled fonts/ folder on the web).  See migration.md 5.28.
 *
 *   The engine's family names are UTF-8.  On Windows guit3's LOGFONT face
 *   names are in the ANSI code page (what the old GDI calls took), so they
 *   are converted on the way through; elsewhere they were always passed
 *   through as they are.
 */
namespace {

std::string face_name_to_utf8(const char *name, size_t len)
{
#ifdef _WIN32
    size_t utf8_len = 0;
    char *utf8 = os_local_to_utf8(0 /* CP_ACP */, name, len, &utf8_len);
    if (utf8 == 0)
        return std::string();
    std::string ret(utf8, utf8_len);
    th_free(utf8);
    return ret;
#else
    return std::string(name, len);
#endif
}

/* false if the name doesn't fit into a LOGFONT, which couldn't find it again */
bool utf8_to_face_name(const std::string &family, char *name, size_t size)
{
#ifdef _WIN32
    size_t len = 0;
    char *local = os_utf8_to_local(0 /* CP_ACP */, family.c_str(), &len);
    if (local == 0)
        return false;
    bool fits = len < size;
    if (fits)
        memcpy(name, local, len + 1);
    th_free(local);
    return fits;
#else
    if (family.size() >= size)
        return false;
    memcpy(name, family.c_str(), family.size() + 1);
    return true;
#endif
}

} // namespace

int os_font_family_is_present(const char *fontname, size_t len)
{
    return CGE::SystemFonts::exists(face_name_to_utf8(fontname, len));
}

/*
 *   'charset' isn't used: the engine matches by family, weight and slant,
 *   and FreeType draws the text in Unicode whatever the font's code pages.
 *   GDI's matching took it into account on Windows before.
 */
unsigned char *os_font_data_for_name(const char *name, int weight, int italic,
                                     int /*charset*/, size_t *data_size,
                                     int *face_index)
{
    if (name == 0)
        name = "";

    CGE::SystemFonts::FontData font;
    if (!CGE::SystemFonts::find(face_name_to_utf8(name, strlen(name)), font,
                                weight, italic != 0)
        || font.data.empty())
        return 0;

    unsigned char *buffer = (unsigned char *)ImGui::MemAlloc(font.data.size());
    if (buffer == 0)
        return 0;
    memcpy(buffer, font.data.data(), font.data.size());
    *data_size = font.data.size();
    if (face_index != 0)
        *face_index = font.faceIndex;
    return buffer;
}

/*
 *   The engine's description of each family, in the ENUMLOGFONTEX/
 *   NEWTEXTMETRIC fields the Customize Theme dialog's selectors read (see
 *   the declaration in tadsfont.h).  'charset_id' is reported back as the
 *   families' character set, apart from symbol fonts (SYMBOL_CHARSET); GDI
 *   used it to leave out the families without that character set before.
 */
void os_enum_font_families(unsigned int charset_id, FONTENUMPROC callback,
                           LPARAM lparam)
{
    std::vector<CGE::SystemFonts::Family> families =
        CGE::SystemFonts::getFamilyDetails();
    for (const CGE::SystemFonts::Family &family : families)
    {
        ENUMLOGFONTEX elf;
        memset(&elf, 0, sizeof(elf));
        if (!utf8_to_face_name(family.name, elf.elfLogFont.lfFaceName,
                               sizeof(elf.elfLogFont.lfFaceName)))
            continue;

        BYTE ff = FF_DONTCARE;
        switch (family.style)
        {
        case CGE::SystemFonts::STYLE_SERIF: ff = FF_ROMAN; break;
        case CGE::SystemFonts::STYLE_SANS_SERIF: ff = FF_SWISS; break;
        case CGE::SystemFonts::STYLE_MONOSPACE: ff = FF_MODERN; break;
        case CGE::SystemFonts::STYLE_SCRIPT: ff = FF_SCRIPT; break;
        case CGE::SystemFonts::STYLE_DECORATIVE: ff = FF_DECORATIVE; break;
        default: break;
        }
        elf.elfLogFont.lfPitchAndFamily = ff;
        elf.elfLogFont.lfCharSet =
            (BYTE)(family.symbol ? SYMBOL_CHARSET : charset_id);

        /* GDI's convention: the bit is set for a proportional font */
        NEWTEXTMETRIC tm;
        memset(&tm, 0, sizeof(tm));
        tm.tmCharSet = elf.elfLogFont.lfCharSet;
        if (!family.fixedPitch)
            tm.tmPitchAndFamily |= TMPF_FIXED_PITCH;

        if (!callback((LOGFONT *)&elf, (TEXTMETRIC *)&tm, 0, lparam))
            break;
    }
}

/*
 *   Make a canonical copy of a LOGFONT structure for comparison purposes
 */
void CTadsFont::copy_canonical_logfont(CTadsLOGFONT *dst,
                                       const CTadsLOGFONT *src)
{
    char *p;
    int rem;

    /* store a copy of the logfont for later comparison */
    memcpy(dst, src, sizeof(*dst));

    /*
     *   Make sure the portion of lfFaceName after the null terminator is
     *   all nulls for easy comparison later.  First, find the null
     *   terminator in the existing name array.  
     */
    for (p = dst->lf.lfFaceName, rem = sizeof(dst->lf.lfFaceName) ;
         *p != 0 && rem > 0 ; ++p, --rem);

    /* now zero out all the bytes from there to the end of the array */
    for ( ; rem > 0 ; ++p, --rem)
        *p = '\0';
}
