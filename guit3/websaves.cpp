/*
 *   websaves.cpp - saved positions and games in the web build (websaves.h)
 *
 *   Emscripten only; CMakeLists.txt adds this file to the web build alone.
 *   guit3 runs on the browser's main thread there (no PROXY_TO_PTHREAD, see
 *   CMakeLists.txt), so the EM_ASM blocks below can use FS directly.
 */

#ifdef __EMSCRIPTEN__

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include <string>

#include <emscripten/emscripten.h>

#include <platform/userdirs.h>

#include "tadsplat.h"
#include "tadshtml.h"
#include "htmlgui.h"
#include "tadswin.h"
#include "tadsfiledlg.h"
#include "websaves.h"

/* a saved position in /IDBFS waits for its commit (see websaves_poll()) */
static bool S_commit_pending = false;

/* what an open file picker uploads: its result is polled */
enum upload_kind_t { UPLOAD_NONE, UPLOAD_SAVE, UPLOAD_GAME };
static upload_kind_t S_upload_kind = UPLOAD_NONE;

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
    if (strncmp(fname_buf, "/IDBFS/", 7) == 0)
        return;

    /* keep the file name only */
    const char *name = strrchr(fname_buf, '/');
    name = (name != 0 ? name + 1 : fname_buf);

    /* with no name, the dialog starts in the folder with the field blank */
    std::string path = websaves_get_dir();
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
 *   Uploads: the browser's file picker.  The JS side writes the chosen file
 *   into 'dir', commits it to IndexedDB, and leaves the outcome in
 *   Module.guit3Upload for websaves_poll(): status 0 = still open,
 *   1 = written (name = the file's name in 'dir'), 2 = cancelled,
 *   3 = failed.
 *
 *   With 'replace' 0, a taken name gets " (2)" etc. before its extension (a
 *   saved position never overwrites another).  With 'replace' 1 the file
 *   replaces one of the same name (a game uploaded again is most likely a
 *   newer release of it); it's written under a temporary name and renamed,
 *   so a game still open from the old file keeps reading the old one until
 *   it ends (MEMFS/IDBFS keep the old node for open streams).
 *
 *   A browser opens a file picker only shortly after a click or a key
 *   (transient user activation).  The menu click that got here normally
 *   still counts; if it doesn't, this returns 0 without trying, and
 *   start_upload() asks for a click in a message box first.
 */
EM_JS(int, websaves_start_picker,
      (const char *dir_c, const char *accept_c, int replace), {
    var dir = UTF8ToString(dir_c);
    if (navigator.userActivation && !navigator.userActivation.isActive)
        return 0;

    var state = { status: 0, name: "" };
    Module.guit3Upload = state;

    var input = document.createElement('input');
    input.type = 'file';
    input.accept = UTF8ToString(accept_c);
    input.style.display = 'none';
    document.body.appendChild(input);

    var finish = function(status, name) {
        if (state.status === 0) {
            state.status = status;
            state.name = name || "";
        }
        if (input.parentNode)
            input.parentNode.removeChild(input);
    };

    input.addEventListener('cancel', function() { finish(2); });
    input.addEventListener('change', function() {
        var file = input.files && input.files[0];
        if (!file) {
            finish(2);
            return;
        }
        file.arrayBuffer().then(function(buf) {
            /* no backslash literals: EM_JS stringifies this code */
            var name = file.name.split('/').join('_')
                .split(String.fromCharCode(92)).join('_');
            if (name === "" || name === '.' || name === '..')
                name = 'upload';
            if (replace) {
                var tmp = dir + '/.upload.tmp';
                FS.writeFile(tmp, new Uint8Array(buf));
                FS.rename(tmp, dir + '/' + name);
            } else {
                var dot = name.lastIndexOf('.');
                var stem = dot > 0 ? name.substring(0, dot) : name;
                var ext = dot > 0 ? name.substring(dot) : "";
                for (var n = 2; FS.analyzePath(dir + '/' + name).exists; ++n)
                    name = stem + ' (' + n + ')' + ext;
                FS.writeFile(dir + '/' + name, new Uint8Array(buf));
            }
            FS.syncfs(false, function(err) {
                if (err)
                    console.error('guit3: saving the upload to IndexedDB failed', err);
            });
            finish(1, name);
        }).catch(function(err) {
            console.error('guit3: uploading a file failed', err);
            finish(3);
        });
    });

    input.click();
    return 1;
});

/*
 *   The upload's status (see above); for status 1, copies the file's name
 *   (UTF-8) to buf.  No upload running counts as cancelled.
 */
EM_JS(int, websaves_poll_upload, (char *buf, int buflen), {
    var state = Module.guit3Upload;
    if (!state)
        return 2;
    if (state.status === 1 && buflen > 0) {
        var bytes = new TextEncoder().encode(state.name);
        var n = Math.min(bytes.length, buflen - 1);
        HEAPU8.set(bytes.subarray(0, n), buf);
        HEAPU8[buf + n] = 0;
    }
    return state.status;
});

/* download the file at 'path' (a browser download under its own name) */
EM_JS(int, websaves_download, (const char *path_c), {
    var path = UTF8ToString(path_c);
    try {
        var data = FS.readFile(path);
        var blob = new Blob([data], { type: 'application/octet-stream' });
        var url = URL.createObjectURL(blob);
        var a = document.createElement('a');
        a.href = url;
        a.download = path.substring(path.lastIndexOf('/') + 1);
        a.style.display = 'none';
        document.body.appendChild(a);
        a.click();
        document.body.removeChild(a);
        setTimeout(function() { URL.revokeObjectURL(url); }, 10000);
        return 1;
    } catch (err) {
        console.error('guit3: exporting ' + path + ' failed', err);
        return 0;
    }
});

/* the folder, file types and texts of an upload kind */
struct upload_info_t
{
    const char *dir;
    const char *accept;
    int replace;
    const char *title;
    const char *ask;
};

static upload_info_t upload_info(upload_kind_t kind)
{
    if (kind == UPLOAD_GAME)
        return { websaves_get_games_dir(), ".t3,.gam", 1,
                 "Open Game from Device",
                 "Choose the game file (.t3 or .gam) to open." };

    return { websaves_get_dir(), ".t3v,.sav", 0,
             "Import Saved Position",
             "Choose the saved position file to import into the "
             "browser's storage." };
}

static void start_upload(CHtmlSys_mainwin *win, upload_kind_t kind)
{
    upload_info_t info = upload_info(kind);
    if (websaves_start_picker(info.dir, info.accept, info.replace))
    {
        S_upload_kind = kind;
        return;
    }

    /* no user activation left: a click on OK gives a fresh one */
    win->queue_deferred_dialog([win, kind, info]()
    {
        if (tadswin_message_box(win->get_app_window(), info.ask, info.title,
                                MB_OKCANCEL | MB_ICONINFORMATION) == IDOK
            && websaves_start_picker(info.dir, info.accept, info.replace))
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

void websaves_export(CHtmlSys_mainwin *win)
{
    /* called from a menu click, inside a frame: the deferred dialog */
    CTadsFileDialog::open(
        TADSFILEDLG_OPEN, "Export Saved Position", S_save_filter,
        websaves_get_dir(), true,
        [win](const char *fname)
        {
            if (fname == 0 || websaves_download(fname))
                return;

            std::string path = fname;
            win->queue_deferred_dialog([win, path]()
            {
                std::string msg = "The file " + path + " couldn't be read.";
                tadswin_message_box(win->get_app_window(), msg.c_str(),
                                    "Export Saved Position",
                                    MB_OK | MB_ICONWARNING);
            });
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

    char name[512];
    int status = websaves_poll_upload(name, sizeof(name));
    if (status == 0)
        return;

    upload_kind_t kind = S_upload_kind;
    S_upload_kind = UPLOAD_NONE;
    upload_info_t info = upload_info(kind);

    if (status == 1 && kind == UPLOAD_GAME)
    {
        /* the current game was confirmed ended before the picker opened */
        std::string path = std::string(info.dir) + "/" + name;
        win->load_new_game(path.c_str());
    }
    else if (status == 1)
    {
        std::string msg = std::string("Imported \"") + name
            + "\". Use File > Restore Position... to load it.";
        win->queue_deferred_dialog([win, msg, info]()
        {
            tadswin_message_box(win->get_app_window(), msg.c_str(),
                                info.title, MB_OK | MB_ICONINFORMATION);
        });
    }
    else if (status == 3)
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
