/*
 *   websaves.cpp - saved positions and games in the web build (websaves.h)
 *
 *   Emscripten only; CMakeLists.txt adds this file to the web build alone.
 *   The uploads and downloads are the engine's (CGE::Platform's file
 *   transfer, platform/services.h).
 */

#ifdef __EMSCRIPTEN__

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include <string>

#include <platform/services.h>
#include <platform/userdirs.h>

#include "tadsplat.h"
#include "tadshtml.h"
#include "htmlgui.h"
#include "tadswin.h"
#include "tadsfiledlg.h"
#include "websaves.h"

/* a saved position in /IDBFS waits for its commit (see websaves_poll()) */
static bool S_commit_pending = false;

/* the transfer that runs (an upload, or an export): its result is polled */
enum upload_kind_t { UPLOAD_NONE, UPLOAD_SAVE, UPLOAD_GAME, UPLOAD_EXPORT };
static upload_kind_t S_upload_kind = UPLOAD_NONE;

/* the file an export downloads */
static std::string S_export_path;

/* the file dialog's filter for saved positions (TADS 3, TADS 2) */
static const char S_save_filter[] =
    "Saved Positions (*.t3v, *.sav)\0*.t3v;*.sav\0"
    "All Files (*.*)\0*.*\0\0";

/* a folder under the IndexedDB user folder, created on the first call */
static const char *user_subdir(std::string &dir, const char *name)
{
    if (dir.empty())
    {
        dir = CGE::UserDirs::getDataDir() + "/" + name;
        mkdir(dir.c_str(), 0777);
    }
    return dir.c_str();
}

const char *websaves_get_dir()
{
    static std::string dir;
    return user_subdir(dir, "saves");
}

const char *websaves_get_games_dir()
{
    static std::string dir;
    return user_subdir(dir, "games");
}

void websaves_fix_default_name(char *fname_buf, size_t fname_buf_len)
{
    /*
     *   a name in the saves folder (the last save) stays; any other, even
     *   one in /IDBFS (an uploaded game's own folder, /IDBFS/guit3/games),
     *   keeps only its file name
     */
    std::string path = websaves_get_dir();
    if (strncmp(fname_buf, path.c_str(), path.size()) == 0
        && fname_buf[path.size()] == '/')
        return;

    /* keep the file name only */
    const char *name = strrchr(fname_buf, '/');
    name = (name != 0 ? name + 1 : fname_buf);

    /* with no name, the dialog starts in the folder with the field blank */
    if (name[0] != '\0')
        path += std::string("/") + name;

    snprintf(fname_buf, fname_buf_len, "%s", path.c_str());
}

void websaves_note_saved(const char *path)
{
    if (strncmp(path, "/IDBFS/", 7) == 0)
        S_commit_pending = true;
}

/* ------------------------------------------------------------------------ */
/*
 *   Uploads and downloads: the engine's file transfer (CGE::Platform's
 *   importFile(), exportFile() and pollTransfer(), platform/services.h).  On
 *   the web an import is the browser's file picker: the chosen file is
 *   written into the folder, committed to IndexedDB, and only then done; an
 *   export is a download.  websaves_poll() polls the outcome.
 *
 *   A saved position never overwrites another: with 'replace' false, a
 *   taken name gets " (2)" etc. before its extension.  An uploaded game
 *   replaces one of the same name (most likely a newer release of it); the
 *   engine writes it under a temporary name and renames it, so a game still
 *   open from the old file keeps reading the old one until it ends
 *   (MEMFS/IDBFS keep the old node for open streams).
 *
 *   A browser opens a file picker only shortly after a click or a key
 *   (transient user activation).  The menu click that got here normally
 *   still counts; if it doesn't, importFile() returns false without trying,
 *   and start_upload() asks for a click in a message box first.
 */

/* the folder, file types and texts of an upload kind */
struct upload_info_t
{
    const char *dir;
    const char *accept;
    bool replace;
    const char *title;
    const char *ask;
};

static upload_info_t upload_info(upload_kind_t kind)
{
    if (kind == UPLOAD_GAME)
        return { websaves_get_games_dir(), ".t3,.gam", true,
                 "Open Game from Device",
                 "Choose the game file (.t3 or .gam) to open." };

    return { websaves_get_dir(), ".t3v,.sav", false,
             "Import Saved Position",
             "Choose the saved position file to import into the "
             "browser's storage." };
}

static void start_upload(CHtmlSys_mainwin *win, upload_kind_t kind)
{
    upload_info_t info = upload_info(kind);
    if (CGE::Platform::importFile(info.dir, info.accept, info.replace))
    {
        S_upload_kind = kind;
        return;
    }

    /* no user activation left: a click on OK gives a fresh one */
    win->queue_deferred_dialog([win, kind, info]()
    {
        if (tadswin_message_box(win->get_app_window(), info.ask, info.title,
                                MB_OKCANCEL | MB_ICONINFORMATION) == IDOK
            && CGE::Platform::importFile(info.dir, info.accept,
                                         info.replace))
            S_upload_kind = kind;
    });
}

void websaves_import(CHtmlSys_mainwin *win)
{
    start_upload(win, UPLOAD_SAVE);
}

void websaves_upload_game(CHtmlSys_mainwin *win)
{
    start_upload(win, UPLOAD_GAME);
}

/* a message box that a file couldn't be exported */
static void export_failed(CHtmlSys_mainwin *win, const std::string &path)
{
    win->queue_deferred_dialog([win, path]()
    {
        std::string msg = "The file " + path + " couldn't be read.";
        tadswin_message_box(win->get_app_window(), msg.c_str(),
                            "Export Saved Position", MB_OK | MB_ICONWARNING);
    });
}

void websaves_export(CHtmlSys_mainwin *win)
{
    /* called from a menu click, inside a frame: the deferred dialog */
    CTadsFileDialog::open(
        TADSFILEDLG_OPEN, "Export Saved Position", S_save_filter,
        websaves_get_dir(), true,
        [win](const char *fname)
        {
            if (fname == 0)
                return;
            if (CGE::Platform::exportFile(fname))
            {
                S_upload_kind = UPLOAD_EXPORT;
                S_export_path = fname;
            }
            else
                export_failed(win, fname);
        });
}

void websaves_poll(CHtmlSys_mainwin *win)
{
    if (S_commit_pending)
    {
        S_commit_pending = false;
        CGE::UserDirs::commit();
    }

    if (S_upload_kind == UPLOAD_NONE)
        return;

    std::string name;
    CGE::Platform::TransferStatus status = CGE::Platform::pollTransfer(&name);
    if (status == CGE::Platform::TRANSFER_RUNNING)
        return;

    upload_kind_t kind = S_upload_kind;
    S_upload_kind = UPLOAD_NONE;
    if (kind == UPLOAD_EXPORT)
    {
        /* a download that couldn't read its file */
        if (status == CGE::Platform::TRANSFER_FAILED)
            export_failed(win, S_export_path);
        return;
    }
    upload_info_t info = upload_info(kind);

    if (status == CGE::Platform::TRANSFER_DONE && kind == UPLOAD_GAME)
    {
        /* the current game was confirmed ended before the picker opened */
        std::string path = std::string(info.dir) + "/" + name;
        win->load_new_game(path.c_str());
    }
    else if (status == CGE::Platform::TRANSFER_DONE)
    {
        std::string msg = std::string("Imported \"") + name
            + "\". Use File > Restore Position... to load it.";
        win->queue_deferred_dialog([win, msg, info]()
        {
            tadswin_message_box(win->get_app_window(), msg.c_str(),
                                info.title, MB_OK | MB_ICONINFORMATION);
        });
    }
    else if (status == CGE::Platform::TRANSFER_FAILED)
    {
        win->queue_deferred_dialog([win, info]()
        {
            tadswin_message_box(win->get_app_window(),
                                "The file couldn't be uploaded.",
                                info.title, MB_OK | MB_ICONWARNING);
        });
    }
}

#endif /* __EMSCRIPTEN__ */
