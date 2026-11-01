/* $Header: d:/cvsroot/tads/html/win32/tadsapp.h,v 1.4 1999/07/11 00:46:47 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1997 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tadsapp.h - TADS Windows application object
Function
  This object provides a repository for data and methods associated
  with the application.
Notes
  
Modified
  09/16/97 MJRoberts  - Creation
*/

#ifndef TADSAPP_H
#define TADSAPP_H

#include "tadsplat.h"

#ifndef TADSHTML_H
#include "tadshtml.h"
#endif
#ifndef TADSFONT_H
#include "tadsfont.h"
#endif

/* ------------------------------------------------------------------------ */
/*
 *   Application object 
 */
class CTadsApp
{
public:
    /*
     *   Run the main event loop.  Returns either when *flag becomes
     *   non-zero, or when the application is terminated by some external
     *   or user action.  If the application is terminated, we'll return
     *   false; if the application should keep running, we'll return true.
     */
    int event_loop(int *flag);

    /*
     *   Apply the standard processing to a message read from the Windows
     *   system event queue.  This gives the message filter, if any, the
     *   first crack at it, then tries the accelerator translation; failing
     *   that, we dispatch the message to the target window normally.  
     */
    void process_message(MSG *msg);

    /* 
     *   Clear the current accelerator if it's associated with the given
     *   window.  This is used when the window is destroyed to ensure that we
     *   clear our memory of its accelerator. 
     */
    void closing_accel_win(class CTadsWin *win);

    /*
     *   Set the current accelerator to a Windows native accelerator
     *   resource.  If accel is not null, we'll use the handle to translate
     *   accelerators and send resulting commands to the indicated window
     *   handle.  
     */
    void set_accel(HACCEL accel, class CTadsWin *win);

    /* set no accelerator */
    void clear_accel();

    /* enable or temporarily disable accelerators */
    int enable_accel(int ena)
    {
        int ret = accel_enabled_;
        accel_enabled_ = ena;
        return ret;
    }

    /* 
     *   translate a message via keyboard accelerators and carry it out;
     *   returns true if we translated the message, false if not 
     */
    int accel_translate(MSG *msg);
    
    /* do idle-time processing */
    void do_idle();

    /* create the application object */
    static void create_app(int argc, char** argv);

    /* get the main application object */
    static CTadsApp *get_app() { return app_; }

    /* delete the application object */
    static void delete_app();

    /* get the application instance handle */
    HINSTANCE get_instance() { return instance_; }

    /* 
     *   Get a font matching a given description.  If we already have a
     *   font matching the description, we'll return it and set *created
     *   to false; otherwise, we'll invoke the callback function (if
     *   provided; if not, we'll use a default creation function) to
     *   create the font, and set *created to true. 
     */
    class CTadsFont *get_font(const CTadsLOGFONT *font,
                              class CTadsFont *
                              (*create_func)(const CTadsLOGFONT *),
                              int *created);

    /* 
     *   Get a font, given a brief description of the font.  The font will
     *   be a proportional roman font in normal window-text color.  This
     *   does the same thing as the version of get_font() based on a
     *   CTadsLOGFONT structure, but is somewhat simpler to call when the
     *   standard defaults can be used in the CTadsLOGFONT structure.  
     */
    class CTadsFont *get_font(const char *font_name, int point_size)
        { return get_font(font_name, point_size, 0, 0,
                          GetSysColor(COLOR_WINDOWTEXT),
                          FF_ROMAN | VARIABLE_PITCH); }

    /* 
     *   Get a font, given a brief description.  'attributes' contains the
     *   Windows attribute flags (a combination of FF_xxx and xxx_PITCH
     *   flags: FF_SWISS | VARIABLE_PITCH for a proportional sans serif
     *   font) to use if the given font name cannot be found.  
     */
    class CTadsFont *get_font(const char *font_name, int point_size,
                              int bold, int italic, int underline,
                              COLORREF fgcolor, int use_fgcolor,
                              COLORREF bgcolor, int use_bgcolor,
                              int attributes);

    class CTadsFont *get_font(const char *font_name, int point_size,
                              int bold, int italic, COLORREF color,
                              int attributes)
    {
        return get_font(font_name, point_size, bold, italic, FALSE,
                        color, TRUE, 0, FALSE, attributes);
    }

    /* create a bold-face version of a given system font */
    static HFONT make_bold_font(HFONT font);

    /*
     *   Get/set the directory for the standard file dialogs.  If the
     *   application wants to keep track of the directory last used for
     *   these dialogs without actually changing the directory, it can use
     *   these operations.  Simply set the lpstrInitialDir member of the
     *   dialog control structure to get_openfile_dir() before calling the
     *   open file function, and then call set_openfile_dir() with the
     *   resulting file to save the directory for next time. 
     */
    char *get_openfile_dir() const { return open_file_dir_; }
    void set_openfile_dir(const char *fname)
        { set_openfile_dir_or_fname(fname, TRUE); }

    /* 
     *   set the openfile directory to a path - this assumes that the
     *   caller already has a directory path without an attached filename 
     */
    void set_openfile_path(const char *fname)
      { set_openfile_dir_or_fname(fname, FALSE); }

    /*
     *   Set the message capture filter.  If this is set, all input will be
     *   sent here before it's processed through the normal Windows
     *   translation and dispatching routines (TranslateAccelerator,
     *   TranslateMessage, DispatchMessage).
     *   
     *   We only maintain one capture filter; callers are responsible for
     *   properly chaining and restoring filters as needed.  For the caller's
     *   convenience in chaining and restoring the older filter, we return
     *   the old filter object.  
     */
    class CTadsMsgFilter *set_msg_filter(class CTadsMsgFilter *filter)
    {
        class CTadsMsgFilter *old_filter;
        
        /* remember the outgoing filter for a moment */
        old_filter = msg_filter_;

        /* set the new filter */
        msg_filter_ = filter;

        /* return the old filter */
        return old_filter;
    }

    /* is this Vista or later? */
    int is_win_vista_plus() const
    {
        return (win_sys_id_ == VER_PLATFORM_WIN32_NT
                && win_ver_major_ >= 6);
    }

    /* determine if the OS is win95 or NT4 (and only those - not later) */
    int is_win95_or_nt4() const
    {
        /* 
         *   win95 is major version 4, minor 0, platform WIN32_WINDOWS; NT4
         *   is major 4, minor 0, platform WIN32_NT 
         */
        return ((win_sys_id_ == VER_PLATFORM_WIN32_WINDOWS
                 && win_ver_major_ == 4 && win_ver_minor_ == 0)
                || (win_sys_id_ == VER_PLATFORM_WIN32_NT
                    && win_ver_major_ == 4 && win_ver_minor_ == 0));
    }

    /* determine if the OS is Windows 2000 (and ONLY Win2k) */
    int is_win2k() const
    {
        return (win_sys_id_ == VER_PLATFORM_WIN32_NT
                && win_ver_major_ == 5
                && win_ver_minor_ == 0);
    }

    /* determine if we're on Windows 2k or later */
    int is_win2k_or_later() const
    {
        return (win_sys_id_ == VER_PLATFORM_WIN32_NT
                && win_ver_major_ >= 5);
    }

    /* get the "My Documents" folder path */
    static int get_my_docs_path(char *fname, size_t fname_len);

    /* keyboard utilities singleton */
    static class CTadsKeyboard *kb_;

    /*
     *   System menu font, in normal and bold renditions 
     */
    static HFONT menu_font;
    static HFONT bold_menu_font;

    /* 
     *   Get this thread's TLS object.  If the thread doesn't have a TLS
     *   object yet, we'll allocate one.  
     */
    static class CTadsAppTls *get_tls_obj();

    /*
     *   Prior to terminating, any thread that called get_tls_obj() must call
     *   this routine to free its TLS object.  
     */
    static void on_thread_exit();

    /*
     *   Register/unregister a status line 
     */
    void register_statusline(class CTadsStatusline *sl);
    void unregister_statusline(class CTadsStatusline *sl);

    /* process a WM_MENUSELECT through the statuslines */
    void send_menuselect_to_statuslines(
        HWND hwnd, WPARAM wparam, LPARAM lparam);

    void setMouseCapture(CTadsWin* win);
	CTadsWin* getMouseCapture() const { return m_mouse_capture_win; }

    /*
     *   Portable substitute for Win32 logical input focus (GetFocus()/
     *   SetFocus()), which no longer means anything in guit3: every
     *   CTadsWin::handle_ is a synthetic, never-registered token
     *   (migration.md 3.4a), so SetFocus(handle_) has always silently done
     *   nothing and GetFocus() never matches it.  CTadsWin::take_focus()
     *   calls setLogicalFocus(this) instead of the dead SetFocus() call;
     *   CHtmlSys_mainwin::get_focus_subwin() (Edit > Copy/Cut/Delete/
     *   SelectAll's dispatch target) reads it back here instead of calling
     *   GetFocus().  Same pattern as m_mouse_capture_win above, just for a
     *   value that persists past mouse-up instead of one that doesn't.
     */
    void setLogicalFocus(CTadsWin* win) { m_logical_focus_win = win; }
    CTadsWin* getLogicalFocus() const { return m_logical_focus_win; }

private:
    CTadsApp();
    CTadsApp(int argc, char** argv);

    virtual ~CTadsApp();

    /* 
     *   internal routine to set the openfile directory; takes a flag
     *   indicating whether we're setting the directory from a directory
     *   name or a filename (in the latter case, we'll extract the path
     *   from the filename) 
     */
    void set_openfile_dir_or_fname(const char *fname, int is_filename);

    /* set the accelerator window, maintaining reference counts */
    void set_accel_win(class CTadsWin *win);

private:
    /* application instance handle */
    HINSTANCE  instance_;

    /* the global application object */
    static CTadsApp *app_;

    /*
     *   font cache - an array of fonts, the number allocated, and the
     *   number actually in use 
     */
    class CTadsFont **fontlist_;
    int fonts_allocated_;
    int fontcount_;

    /* open-file directory */
    char *open_file_dir_;

    /* message filter */
    class CTadsMsgFilter *msg_filter_;

    /* the list of registered menu status handlers */
    struct statline_listele *statusline_;

    /* flag indicating that OLE was initialized successfully */
    unsigned int ole_inited_ : 1;

    /* current accelerator and accelerator target window */
    HACCEL waccel_;
    class CTadsWin *accel_win_;

    /* are accelerators enabled? */
    int accel_enabled_;

    /* Windows OS system identifier (tells us if we're on 95, NT, etc) */
    unsigned long win_sys_id_;

    /* major and minor Windows OS version identifiers */
    unsigned long win_ver_major_;
    unsigned long win_ver_minor_;

    /* the thread local storage (TLS) index of our thread context structure */
    static DWORD tls_index_;

	CTadsWin* m_mouse_capture_win;

    /* portable logical-focus tracker - see setLogicalFocus()/getLogicalFocus() */
    CTadsWin* m_logical_focus_win;
};


/* ------------------------------------------------------------------------ */
/*
 *   Thread-Local Storage object.  We allocate one of these per thread, to
 *   store various global variables for the thread. 
 */
class CTadsAppTls
{
public:
    CTadsAppTls()
    {
        /* clear the object */
        memset(this, 0, sizeof(*this));
    }

    /* current window creation hook object, if any */
    class CTadsWinCreateHook *chook;
};


/* ------------------------------------------------------------------------ */
/*
 *   Message filter interface - for use with CTadsApp::set_msg_filter().
 */
class CTadsMsgFilter
{
public:
    /* 
     *   Process a system message.  The message filter object always gets
     *   first crack at a message, before we pass it to any of the standard
     *   system handlers (TranslateAccelerator,
     *   TranslateMessage, DispatchMessage).  Return true to indicate that
     *   the message was handled - we'll do nothing further with it.  Return
     *   false to indicate that the standard handling should proceed.  
     */
    virtual int filter_system_message(MSG *msg) = 0;
};


/* ------------------------------------------------------------------------ */
/*
 *   OPENFILENAME structure with and without the Win 2k+ expansion.  We want
 *   our code to work on NT4/95/98/Me as well, so we need to use the larger
 *   structure on the newer systems and the smaller structure on the older
 *   systems.  The whole point of the lStructSize field is that it *should*
 *   take care of this for us automatically by letting the APIs know which
 *   version of the structure we're using, but alas, things never quite work
 *   as documented.  
 */
#if _WIN32_WINNT < 0x0500
#define OPENFILENAME_NT4  OPENFILENAME
#endif
struct OPENFILENAME5: OPENFILENAME_NT4
{
    /* the following are the NT5 extensions */
    void        *pvReserved;
    DWORD        dwReserved;
    DWORD        FlagsEx;

    OPENFILENAME5()
    {
        memset(this, 0, sizeof(*this));
        lStructSize = CTadsApp::get_app()->is_win2k_or_later()
                      ? sizeof(OPENFILENAME5) : sizeof(OPENFILENAME_NT4);
    }
};

#endif /* TADSAPP_H */

