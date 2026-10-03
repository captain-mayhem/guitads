/*
 *   Copyright (c) 2026 by the TADS 3 authors.  All Rights Reserved.
 *
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.
 */
/*
Name
  guifont.h - tads imgui font implementation
Function
  
Notes
  
Modified
  01/31/98 MJRoberts  - Creation
*/

#ifndef W32FONT_H
#define W32FONT_H

#ifndef HTMLSYS_H
#include "htmlsys.h"
#endif
#ifndef TADSFONT_H
#include "tadsfont.h"
#endif


/* ------------------------------------------------------------------------ */
/*
 *   System font object 
 */
class CHtmlSysFont_win32: public CTadsFont, public CHtmlSysFont
{
public:
    CHtmlSysFont_win32(const CTadsLOGFONT *lf);
    ~CHtmlSysFont_win32();

    /* get metrics */
    void get_font_metrics(class CHtmlFontMetrics *);

    /* is this a fixed-pitch font? */
    int is_fixed_pitch() { return is_fixed_pitch_; }

    /* get the font's em size */
    int get_em_size() { return em_size_; }

    /* set the font descriptor */
    void set_font_desc(const class CHtmlFontDesc *src)
    {
        /* copy the descriptor */
        desc_.copy_from(src);

        /* 
         *   clear the explicit-face-name flag, since this is important only
         *   when looking up a font 
         */
        desc_.face_set_explicitly = FALSE;
    }

private:
    /* get my FreeType-baked glyph metrics at my configured pixel size */
    struct ImFontBaked *get_baked();

    /* am I a fixed-pitch (monospaced) font? */
    int is_fixed_pitch_;

    /* my em size, in pxels */
    int em_size_;
};


#endif /* W32FONT_H */
