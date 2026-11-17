/*
 *   urlgame.h - File > Open Game from Web... (all platforms)
 *
 *   Asks for the address (URL) of a game file, downloads it into the games
 *   folder below the user's data folder (CGE::UserDirs::getDataDir() +
 *   "/games"; on the web that is /IDBFS/guit3/games, the folder Open Game
 *   from Device... uploads to, so the game survives a reload) and loads it
 *   with CHtmlSys_mainwin::load_new_game(path), which also puts it in the
 *   Recent Games list.  A file of the same name is replaced (most likely a
 *   newer release of the same game).
 *
 *   The download is the engine's (CGE::Platform::downloadFile(), reported
 *   through pollTransfer() like an upload): WinINet on Windows, the curl
 *   (or wget) program on Linux, HttpURLConnection on Android and fetch() on
 *   the web - where the server must allow the page's origin (CORS).
 *
 *   The prompt and the "Downloading..." box are ImGui modals drawn each
 *   frame by urlgame_render(), which also polls the download.  See
 *   migration.md 5.40.
 */

#ifndef URLGAME_H
#define URLGAME_H

/* can this platform download games at all (the menu item's state) */
bool urlgame_available();

/* the downloaded games' folder (created on the first call) */
const char *urlgame_get_dir();

/*
 *   Show the URL prompt.  The caller has already confirmed ending the
 *   current game (CHtmlSys_mainwin::load_url_game()).
 */
void urlgame_open(class CHtmlSys_mainwin *win);

/* draw the prompt or the download's progress, and poll the download */
void urlgame_render(class CHtmlSys_mainwin *win);

#endif /* URLGAME_H */
