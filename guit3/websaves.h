/*
 *   websaves.h - saved positions in the web build (Emscripten only)
 *
 *   The web build's file system is in memory, except for /IDBFS, which the
 *   engine keeps in the browser's IndexedDB (CGE::UserDirs).  Saved
 *   positions go to a folder there, so they survive a reload, and two File
 *   menu commands move them in and out of the browser:
 *
 *   - Export Saved Position... downloads a file (a browser download)
 *   - Import Saved Position... uploads one (the browser's file picker) into
 *     the saves folder, where Restore Position... finds it
 *
 *   A third, Open Game from Device..., uploads a game file into
 *   /IDBFS/guit3/games and loads it, so it stays in the Recent Games list
 *   (and in Open New Game...'s dialog) after a reload.
 *
 *   Files written to /IDBFS only reach IndexedDB with UserDirs::commit().
 *   os_askfile() returns before the game writes the file, so the commit is
 *   left to websaves_poll(), which event_loop() calls between frames - by
 *   then the game is waiting for input again and the file is complete.
 *
 *   See migration.md 5.35.
 */

#ifndef WEBSAVES_H
#define WEBSAVES_H

#ifdef __EMSCRIPTEN__

#include <stddef.h>

namespace CGE { class AppWindow; }

/* the saves folder, /IDBFS/guit3/saves (created on the first call) */
const char *websaves_get_dir();

/*
 *   Point an os_askfile() default name into the saves folder: a name
 *   outside /IDBFS (the game's own folder, /tmp, ...) would be lost on a
 *   reload, so only its file name is kept.  A name already in /IDBFS (the
 *   last save) stays as it is.
 */
void websaves_fix_default_name(char *fname_buf, size_t fname_buf_len);

/* a position was saved to 'path': commit it to IndexedDB on the next poll */
void websaves_note_saved(const char *path);

/*
 *   Between-frames work, called by event_loop(): commits saved positions,
 *   and reports a finished import with a message box on 'win'
 */
void websaves_poll(class CHtmlSys_mainwin *win);

/* File > Export Saved Position...: pick a file, then download it */
void websaves_export(class CHtmlSys_mainwin *win);

/* File > Import Saved Position...: the browser's file picker */
void websaves_import(class CHtmlSys_mainwin *win);

/* the uploaded games' folder, /IDBFS/guit3/games (created on the first call) */
const char *websaves_get_games_dir();

/*
 *   File > Open Game from Device...: the browser's file picker; the
 *   uploaded game is loaded with win->load_new_game(path) from
 *   websaves_poll().  The caller has already confirmed ending the current
 *   game (CHtmlSys_mainwin::load_uploaded_game()).
 */
void websaves_upload_game(class CHtmlSys_mainwin *win);

#endif /* __EMSCRIPTEN__ */

#endif /* WEBSAVES_H */
