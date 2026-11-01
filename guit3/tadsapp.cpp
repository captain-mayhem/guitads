#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/html/win32/tadsapp.cpp,v 1.4 1999/07/11 00:46:47 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1997 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tadsapp.cpp - TADS Windows application object
Function
  
Notes
  
Modified
  09/16/97 MJRoberts  - Creation
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <ctype.h>

#ifdef _WIN32
#include <Ole2.h>
#include <Windows.h>
#include <shlobj.h>
#else
#include "tadsplat.h"
#include <pthread.h>

/* portable stand-ins for the four Win32 TLS calls this file uses to store a
   per-thread CTadsAppTls* (tls_index_ is a DWORD "slot index" either way) */
static DWORD TlsAlloc()
{
    pthread_key_t key;
    pthread_key_create(&key, 0);
    return (DWORD)key;
}
static void TlsFree(DWORD idx) { pthread_key_delete((pthread_key_t)idx); }
static void *TlsGetValue(DWORD idx) { return pthread_getspecific((pthread_key_t)idx); }
static void TlsSetValue(DWORD idx, void *val)
    { pthread_setspecific((pthread_key_t)idx, val); }
#endif

#ifndef TADSAPP_H
#include "tadsapp.h"
#endif
#ifndef TADSWIN_H
#include "tadswin.h"
#endif
#ifndef TADSFONT_H
#include "tadsfont.h"
#endif
#ifndef TADSSTAT_H
#include "tadsstat.h"
#endif
#ifndef TADSKB_H
#include "tadskb.h"
#endif
#ifndef HTMLHASH_H
#include "htmlhash.h"
#endif
#ifndef TADSMIDI_H
#include "tadsmidi.h"
#endif
#include "htmlgui.h"
#include "htmlprs.h"
#include <imgui.h>
#include <imgui_internal.h>


/* ------------------------------------------------------------------------ */
/*
 *   Application object 
 */

/* static containing the app object singleton */
CTadsApp *CTadsApp::app_ = 0;

/* system menu font, in normal and bold renditions */
HFONT CTadsApp::menu_font = 0;
HFONT CTadsApp::bold_menu_font = 0;

/* the TLS index for our pre-thread storage object */
DWORD CTadsApp::tls_index_ = 0;

/* the keyboard utilities singleton */
CTadsKeyboard *CTadsApp::kb_ = 0;

/* registered statusline list entry */
struct statline_listele
{
    statline_listele *nxt;
    CTadsStatusline *stat;

    statline_listele(CTadsStatusline *s, statline_listele *n)
        { stat = s; nxt = n; }
};

/*
 *   initialize 
 */
CTadsApp::CTadsApp(int argc, char** argv)
{
    OSVERSIONINFO osver;

    /* remember the application instance handle */
#ifdef _WIN32
    instance_ = GetModuleHandle(NULL);
#else
    instance_ = 0;
#endif

    /* no fonts allocated yet */
    fonts_allocated_ = 0;
    fontlist_ = 0;
    fontcount_ = 0;

    /* no accelerators yet */
    waccel_ = 0;
    accel_win_ = 0;
    accel_enabled_ = TRUE;

    /* no registered statuslines yet */
    statusline_ = 0;

    /* initialize OLE */
#ifdef _WIN32
    switch(OleInitialize(0))
    {
    case S_OK:
    case S_FALSE:
        /*
         *   note that OLE was initialized, so we'll need to uninitialize
         *   it before we shut down
         */
        ole_inited_ = TRUE;
        break;

    default:
        /* OLE didn't initialize properly */
        ole_inited_ = FALSE;
        break;
    }
#else
    /* no COM/OLE off Windows - tadsole.cpp's drag-and-drop source is
       Windows-only (migration.md 5.3/5.4/O) */
    ole_inited_ = FALSE;
#endif

    /* allocate our thread local storage index */
    tls_index_ = TlsAlloc();

    /* no file-dialog directory yet */
    open_file_dir_ = 0;

    /* we don't have a message filter installed yet */
    msg_filter_ = 0;

    /* no window currently has mouse capture */
    m_mouse_capture_win = 0;

    /* no window currently has logical focus */
    m_logical_focus_win = 0;

    /* get OS version information */
    memset(&osver, 0, sizeof(osver));
    osver.dwOSVersionInfoSize = sizeof(osver);
#ifdef _WIN32
    GetVersionEx(&osver);
#endif

    /* note the windows version identifiers */
    win_sys_id_ = osver.dwPlatformId;
    win_ver_major_ = osver.dwMajorVersion;
    win_ver_minor_ = osver.dwMinorVersion;

    /* register the basic window class */
    CTadsWin::register_win_class(this);

    /* perform static class initialization */
    CTadsWin::class_init(this);
    CTadsWinScroll::class_init(this);

    /* get the system menu font */
    {
        NONCLIENTMETRICS ncm;
        ncm.cbSize = sizeof(ncm);

        /* 
         *   get the font from the system parameters, which works for newer
         *   versions of Windows; if that doesn't work, we're on an older
         *   version, so simply use the DEFAULT_GUI_FONT stock object 
         */
        if (SystemParametersInfo(
            SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0))
            menu_font = CreateFontIndirect(&ncm.lfMenuFont);
        else
            menu_font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    }

    /* create a boldface version of the system menu font */
    bold_menu_font = make_bold_font(menu_font);

    /* set up our keyboard utilities singleton */
    kb_ = new CTadsKeyboard();
}

CTadsApp::~CTadsApp()
{
    int i;
    CTadsFont **p;
    
    /* perform static termination */
    CTadsWin::class_terminate();
    CTadsWinScroll::class_terminate();
    CTadsMidiFilePlayer::class_terminate();

    /* done with our keyboard utilities object */
    delete kb_;

    /* delete any fonts we've allocated */
    for (p = fontlist_, i = 0 ; i < fontcount_ ; ++i, ++p)
        delete *p;

    /* delete the font list */
    if (fontlist_)
    {
        th_free(fontlist_);
        fontlist_ = 0;
        fontcount_ = 0;
        fonts_allocated_ = 0;
    }

    /* delete the open-file buffer, if we have one */
    if (open_file_dir_ != 0)
        th_free(open_file_dir_);

    /* deinitialize OLE if necessary */
#ifdef _WIN32
    if (ole_inited_)
        OleUninitialize();
#endif

    /* delete the main thread's TLS object */
    on_thread_exit();

    /* delete our TLS index */
    TlsFree(tls_index_);
}

/*
 *   Get my thread-local storage object 
 */
CTadsAppTls *CTadsApp::get_tls_obj()
{
    CTadsAppTls *obj;
    
    /* get the existing object; if we have one, simply return it */
    if ((obj = (CTadsAppTls *)TlsGetValue(tls_index_)) != 0)
        return obj;

    /* we don't have one - allocate one */
    obj = new CTadsAppTls();

    /* store it */
    TlsSetValue(tls_index_, obj);

    /* return it */
    return obj;
}

/*
 *   Exit a thread.  This deletes the TLS object, if any, associated with the
 *   thread. 
 */
void CTadsApp::on_thread_exit()
{
    /* if we have a TLS object for this thread, delete it */
    CTadsAppTls *obj;

    /* get the existing object; if we have one, simply return it */
    if ((obj = (CTadsAppTls *)TlsGetValue(tls_index_)) != 0)
    {
        /* clear our TLS slot */
        TlsSetValue(tls_index_, 0);

        /* delete the object */
        delete obj;
    }
}

/*
 *   create the application object 
 */
void CTadsApp::create_app(int argc, char** argv)
{
    /* if there's not already an app, create one */
    if (app_ == 0)
        app_ = new CTadsApp(argc, argv);
}

/*
 *   delete the application object 
 */
void CTadsApp::delete_app()
{
    if (app_ != 0)
    {
        delete app_;
        app_ = 0;
    }
}

/*
 *   main event loop
 */
int CTadsApp::event_loop(int *flag)
{
    BOOL stat;
    
    /* if there's a flag, initialize it to false */
    if (flag != 0)
        *flag = 0;

    CHtmlSysFrame* frame = CHtmlSysFrame::get_frame_obj();
    CHtmlSys_mainwin* win = static_cast<CHtmlSys_mainwin*>(frame);
    if (win)
        return win->event_loop(flag);
    else
        return FALSE;
}

/*
 *   Apply our standard message processing to a message read from the Windows
 *   system event queue.  
 */
void CTadsApp::process_message(MSG *msg)
{
 int done = FALSE;

    /* 
     *   If there's a message filter, give it the first shot at the message.
     *   If the message filter marks the message as handled, then skip all
     *   remaining processing on it.  
     */
    if (msg_filter_ != 0 && msg_filter_->filter_system_message(msg))
        done = TRUE;
    
    /* try the accelerators, if they're enabled */
    if (!done && accel_translate(msg))
        done = TRUE;
    
    /* if we didn't already handle it, dispatch it normally */
    if (!done)
    {
        /* translate and dispatch the message */
        TranslateMessage(msg);
        DispatchMessage(msg);
    }
}


/*
 *   Translate and execute a command via accelerators
 */
int CTadsApp::accel_translate(MSG *msg)
{
    /* if accelerators are enabled, try the windows native accelerator */
    return (accel_enabled_
            && waccel_ != 0
            && TranslateAccelerator(accel_win_->get_handle(), waccel_, msg));
}

/* ------------------------------------------------------------------------ */
/*
 *   Register a statusline 
 */
void CTadsApp::register_statusline(class CTadsStatusline *sl)
{
    /* link in a new list entry for the statusline */
    statusline_ = new statline_listele(sl, statusline_);
}

/*
 *   unregister a statusline 
 */
void CTadsApp::unregister_statusline(class CTadsStatusline *sl)
{
    statline_listele *cur, *prv;

    /* find the list element */
    for (prv = 0, cur = statusline_ ; cur != 0 ; prv = cur, cur = cur->nxt)
    {
        /* if this is the one, unlink it */
        if (cur->stat == sl)
        {
            /* unlink it */
            if (prv == 0)
                statusline_ = cur->nxt;
            else
                prv->nxt = cur->nxt;

            /* delete this entry */
            delete cur;

            /* that's all we need to do */
            return;
        }
    }
}

/*
 *   Process a WM_MENUSELECT through each status line 
 */
void CTadsApp::send_menuselect_to_statuslines(
    HWND hwnd, WPARAM wparam, LPARAM lparam)
{
    statline_listele *cur;
    
    /* pass it to each status line */
    for (cur = statusline_ ; cur != 0 ; cur = cur->nxt)
        cur->stat->menu_select_msg(hwnd, wparam, lparam);
}


/* ------------------------------------------------------------------------ */
/*
 *   idle-time processing 
 */
void CTadsApp::do_idle()
{
}

/* ------------------------------------------------------------------------ */
/*
 *   Set the active accelerator 
 */
void CTadsApp::set_accel(HACCEL accel, class CTadsWin *win)
{
    /* remember the new accelerator settings */
    waccel_ = accel;
    set_accel_win(win);
}

void CTadsApp::clear_accel()
{
    waccel_ = 0;
    set_accel_win(0);
}

/* set the accelerator window */
void CTadsApp::set_accel_win(CTadsWin *win)
{
    /* add a reference to the new window */
    if (win != 0)
        win->AddRef();

    /* release any previous accelerator window */
    if (accel_win_ != 0)
        accel_win_->Release();

    /* remember the new window */
    accel_win_ = win;
}

/* receive notification that a window with an accelerator is closing */
void CTadsApp::closing_accel_win(CTadsWin *win)
{
    /* if it's our current accelerator window, forget it */
    if (accel_win_ == win)
    {
        /* forget our accelerator */
        waccel_ = 0;
        set_accel_win(0);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Get a font given the font description.
 */
class CTadsFont *CTadsApp::get_font(const char *font_name, int point_size,
                                    int bold, int italic, int underline,
                                    COLORREF fgcolor, int use_fgcolor,
                                    COLORREF bgcolor, int use_bgcolor,
                                    int attributes)
{
    CTadsLOGFONT tlf;

    /* set up the font description structure */
    tlf.lf.lfWidth = 0;
    tlf.lf.lfEscapement = 0;
    tlf.lf.lfOrientation = 0;
    tlf.lf.lfCharSet = DEFAULT_CHARSET;
    tlf.lf.lfOutPrecision = OUT_DEFAULT_PRECIS;
    tlf.lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    tlf.lf.lfQuality = DEFAULT_QUALITY;
    tlf.lf.lfHeight = CTadsFont::calc_lfHeight(point_size);
    tlf.lf.lfItalic = (italic != 0);
    tlf.lf.lfUnderline = (underline != 0);
    tlf.lf.lfStrikeOut = 0;
    tlf.superscript = 0;
    tlf.subscript = 0;
    tlf.color = fgcolor;
    tlf.color_is_input = FALSE;
    tlf.color_set_explicitly = use_fgcolor;
    tlf.bgcolor = bgcolor;
    tlf.bgcolor_set = use_bgcolor;
    tlf.lf.lfWeight = (bold ? FW_BOLD : FW_NORMAL);
    tlf.lf.lfPitchAndFamily = (BYTE)attributes;
    tlf.codepage = GetACP();

    /* copy the font name if provided */
    if (font_name == 0)
    {
        tlf.face_set_explicitly = FALSE;
        tlf.lf.lfFaceName[0] = '\0';
    }
    else
    {
        tlf.face_set_explicitly = TRUE;
        strcpy(tlf.lf.lfFaceName, font_name);
    }
    
    /* get the font */
    return get_font(&tlf, 0, 0);
}


/*
 *   Get a font given a description.  We'll try to find a matching font
 *   in our cache, and return that object; if we don't already have a font
 *   matching this description, we'll create a new one and add it to our
 *   list.  
 */
class CTadsFont *CTadsApp::get_font(
    const CTadsLOGFONT *logfont,
    CTadsFont *(*create_func)(const CTadsLOGFONT *),
    int *created)
{
    int i;
    CTadsFont **p;
    CTadsFont *newfont;
    
    /* run through my list of fonts and see if we can find an exact match */
    for (p = fontlist_, i = 0 ; i < fontcount_ ; ++i, ++p)
    {
        /* if this font matches the description, return it */
        if ((*p)->matches(logfont))
        {
            if (created != 0)
                *created = FALSE;
            return *p;
        }
    }

    /* we'll need to create a new font - ensure the table is big enough */
    if (fonts_allocated_ == fontcount_)
    {
        /* expand the table */
        fonts_allocated_ += 100;
        if (fontlist_ == 0)
            fontlist_ = (CTadsFont **)th_malloc(fonts_allocated_
                                                * sizeof(CTadsFont *));
        else
            fontlist_ = (CTadsFont **)th_realloc(fontlist_,
                fonts_allocated_ * sizeof(CTadsFont *));
    }

    /* create a new font */
    if (created != 0)
        *created = TRUE;
    newfont = (create_func != 0 ? (*create_func)(logfont)
                           : new CTadsFont(logfont));

    /* add it to the table */
    fontlist_[fontcount_++] = newfont;

    /* return it */
    return newfont;
}

/*
 *   Create a bold version of a given font
 */
HFONT CTadsApp::make_bold_font(HFONT font)
{
    LOGFONT lf;
    HDC dc = GetWindowDC(0);

    /* clear out the LOGFONT for starters */
    memset(&lf, 0, sizeof(lf));

    /* select the font */
    HFONT oldfont = (HFONT)SelectObject(dc, font);

    /* get the text metrics for the font */
    TEXTMETRIC tm;
    GetTextMetrics(dc, &tm);

    /* get the font name */
    GetTextFace(dc, LF_FACESIZE, lf.lfFaceName);

    /* set up the LOGFONT from the TEXTMETRIC */
    lf.lfHeight = tm.tmHeight;
    lf.lfWeight = tm.tmWeight + 300;
    lf.lfCharSet = tm.tmCharSet;
    lf.lfOutPrecision = OUT_DEFAULT_PRECIS;
    lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    lf.lfQuality = DEFAULT_QUALITY;
    lf.lfPitchAndFamily = tm.tmPitchAndFamily;
    
    /* create the font */
    HFONT bf = CreateFontIndirect(&lf);

    /* clean up */
    SelectObject(dc, oldfont);
    ReleaseDC(0, dc);

    /* return the new font */
    return bf;
}

/*
 *   Set the open-file directory to the directory of the given file
 */
#ifndef _WIN32
/* oss_set_open_file_dir() (tads2/msdos/oswin.h) has no non-Windows
   declaration - the unix/osunixt.c build of Tads::tr32h doesn't implement
   this Windows-only "remember the last directory for the next native
   GetOpenFileName() call" hook (migration.md 5.1). CTadsFileDialog (the
   live ImGui file-open dialog) tracks its own state instead, so this is a
   no-op here. */
inline void oss_set_open_file_dir(const char *) { }
#endif

void CTadsApp::set_openfile_dir_or_fname(const char *fname, int is_filename)
{
    size_t fname_len;

    /* if we were given a filename, find the path prefix */
    if (is_filename)
    {
        const char *last_sep;
        const char *p;

        /* find the last path separator in the filename */
        for (p = fname, last_sep = 0 ; *p != 0 ; ++p)
        {
            /* if this is a path separator, remember it */
            if (*p == ':' || *p == '/' || *p == '\\')
                last_sep = p;
        }
        
        /* see if we found a separator */
        if (last_sep == 0)
        {
            /* 
             *   no separators at all - the file is in the current
             *   directory, so there's nothing we need to do 
             */
            return;
        }

        /* the length we want is that of the path prefix portion only */
        fname_len = last_sep - fname;
    }
    else
    {
        /* we were given a directory name - use the entire string */
        fname_len = strlen(fname);
    }

    /* if we don't have a buffer yet, allocate one */
    if (open_file_dir_ == 0)
        open_file_dir_ = (char *)th_malloc(256);

    /* save everything up to the last separator */
    memcpy(open_file_dir_, fname, fname_len);
    open_file_dir_[fname_len] = '\0';

    /* tell the OS layer to use this as the open file directory */
    oss_set_open_file_dir(open_file_dir_);
}

/* ------------------------------------------------------------------------ */
/*
 *   Get the "My Documents" folder path 
 */
#ifndef _WIN32
/*
 *   The documents folder off Windows: XDG_DOCUMENTS_DIR from the user's
 *   $XDG_CONFIG_HOME/user-dirs.dirs (what the desktop's file manager calls
 *   Documents, e.g. $HOME/Documents or a translated name), if that folder
 *   exists; otherwise $HOME.  (On the web $HOME is /home/web_user.)
 */
static int get_xdg_docs_path(char *fname, size_t fname_len, const char *home)
{
    char path[OSFNMAX];
    const char *cfg = getenv("XDG_CONFIG_HOME");
    if (cfg != 0 && cfg[0] != '\0')
        os_build_full_path(path, sizeof(path), cfg, "user-dirs.dirs");
    else
    {
        char dir[OSFNMAX];
        os_build_full_path(dir, sizeof(dir), home, ".config");
        os_build_full_path(path, sizeof(path), dir, "user-dirs.dirs");
    }

    FILE *fp = fopen(path, "r");
    if (fp == 0)
        return FALSE;

    /* the line is XDG_DOCUMENTS_DIR="$HOME/Documents" (shell syntax) */
    int found = FALSE;
    char line[OSFNMAX + 64];
    while (!found && fgets(line, sizeof(line), fp) != 0)
    {
        static const char key[] = "XDG_DOCUMENTS_DIR=\"";
        if (strncmp(line, key, sizeof(key) - 1) != 0)
            continue;
        char *val = line + sizeof(key) - 1;
        char *end = strchr(val, '"');
        if (end == 0)
            break;
        *end = '\0';

        /* $HOME/ at the start is the only variable the file uses */
        if (strncmp(val, "$HOME", 5) == 0 && (val[5] == '/' || val[5] == '\0'))
            snprintf(fname, fname_len, "%s%s", home, val + 5);
        else if (val[0] == '/')
            snprintf(fname, fname_len, "%s", val);
        else
            break;

        unsigned long mode;
        found = osfmode(fname, TRUE, &mode, 0) && (mode & OSFMODE_DIR) != 0;
    }
    fclose(fp);
    return found;
}
#endif

int CTadsApp::get_my_docs_path(char *fname, size_t fname_len)
{
#ifndef _WIN32
    const char *home = getenv("HOME");
    if (home == 0 || home[0] == '\0')
    {
        fname[0] = '\0';
        return FALSE;
    }
    if (!get_xdg_docs_path(fname, fname_len, home))
        snprintf(fname, fname_len, "%s", home);
    return TRUE;
#else
    IMalloc *imal;
    ITEMIDLIST *idlist;

    /* get a windows shell ITEMIDLIST for the "My Documents" folder */
    if (!SUCCEEDED(SHGetSpecialFolderLocation(0, CSIDL_PERSONAL, &idlist)))
    {
        /* couldn't get the folder location - return an empty path */
        fname[0] = '\0';
        return FALSE;
    }

    /* convert the ID list to a path */
    SHGetPathFromIDList(idlist, fname);

    /* done with the ITEMIDLIST - free it using the shell's allocator */
    SHGetMalloc(&imal);
    imal->Free(idlist);
    imal->Release();

    /* indicate success */
    return TRUE;
#endif
}

void CTadsApp::setMouseCapture(CTadsWin* win) {
    m_mouse_capture_win = win;
    if (ImGui::GetCurrentContext()->CurrentWindow != NULL)
        return;
    if (win)
        SetCapture(win->get_handle());
    else
        ReleaseCapture();
}
