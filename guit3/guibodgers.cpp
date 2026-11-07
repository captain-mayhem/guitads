/*
 *   guibodgers.cpp - the "bodgers" function set.  See guibodgers.h.
 */

#include <string>

#include "t3std.h"
#include "vmglob.h"
#include "vmtype.h"
#include "vmstack.h"
#include "vmrun.h"
#include "vmbif.h"
#include "vmimage.h"
#include "vmstr.h"
#include "vmobj.h"
#include "charmap.h"
#include "os.h"

#include <platform/systemfonts.h>

#include "htmlgui.h"
#include "htmlpref.h"
#include "htmlver.h"
#include "guiver.h"
#include "guimap.h"
#include "guibodgers.h"

namespace {

/* the game's folder (absolute, without a separator at the end) */
std::string game_dir;

/* return a local filename as a string */
void retval_fname(VMG_ const char *fname)
{
    size_t len = G_cmap_from_fname->map_str(0, 0, fname);
    vm_obj_id_t id = CVmObjString::create(vmg_ FALSE, len);
    CVmObjString *str = (CVmObjString *)vm_objp(vmg_ id);
    G_cmap_from_fname->map_str(str->cons_get_buf(), len, fname);
    CVmBif::retval_obj(vmg_ id);
}

/* the game's folder, with a separator to append file names to */
std::string game_dir_slash()
{
    std::string dir = game_dir;
    if (!dir.empty() && dir.back() != '/' && dir.back() != '\\')
        dir += OSPATHCHAR;
    return dir;
}

/*
 *   The look of the game's own front end, as a theme: the Dark theme's
 *   colors (its palette) with the fonts the game brings along in fonts/,
 *   at the front end's default sizes.  Created once; a game without a
 *   theme of its own gets it.
 */
void set_up_theme()
{
    static const char *const theme = "Thaumistry";
    CHtmlSys_mainwin *win = CHtmlSys_mainwin::get_main_win();
    if (win == 0)
        return;
    CHtmlPreferences *prefs = win->get_prefs();
    if (!CHtmlPreferences::profile_exists(theme))
    {
        /* build it in the preferences, then go back to the active theme */
        prefs->save();
        prefs->set_theme_defaults("Dark");
        prefs->set_prop_font("Open Sans");
        prefs->set_prop_fontsz(14);
        prefs->set_mono_font("Cousine");
        prefs->set_mono_fontsz(14);
        prefs->set_font_serif("Gunny Rewritten");
        prefs->set_serif_fontsz(22);
        prefs->set_font_sans("Open Sans");
        prefs->set_sans_fontsz(14);
        prefs->set_font_script("Mrs Saint Delafield");
        prefs->set_script_fontsz(34);
        prefs->set_font_typewriter("Erica Type");
        prefs->set_typewriter_fontsz(16);
        prefs->set_inpfont_name("");
        prefs->set_inpfont_size(14);
        prefs->set_inpfont_bold(FALSE);
        prefs->set_inpfont_italic(TRUE);
        prefs->set_profile_desc(
            "The look of Thaumistry's own interpreter: its fonts, from the "
            "game's fonts folder, with the Dark theme's colors.");
        prefs->save_as(theme);
        prefs->restore(FALSE);
    }
    win->suggest_game_profile(theme);
}

}

/* ------------------------------------------------------------------------ */
/*
 *   Loading and ending the game
 */
void CVmBifBodgers::attach(VMG0_)
{
    /* the loader is there while the image loads (and gone afterwards) */
    game_dir.clear();
    if (G_image_loader != 0 && G_image_loader->get_path() != 0)
        game_dir = G_image_loader->get_path();
    guimap_set_game(game_dir.c_str());

    /* the game's fonts, before the theme that uses them */
    if (!game_dir.empty())
        CGE::SystemFonts::addFolder(game_dir_slash() + "fonts");
    set_up_theme();
}

void CVmBifBodgers::detach(VMG0_)
{
    guimap_set_game(0);
    game_dir.clear();
}

/* ------------------------------------------------------------------------ */
/*
 *   The functions
 */

/* function 0 lost its name; whatever it gets, it does nothing */
void CVmBifBodgers::deprecated(VMG_ uint argc)
{
    G_stk->discard(argc);
    retval_nil(vmg0_);
}

/*
 *   The front end's main menu (resume, load, save, options, credits, quit).
 *   guit3 has these in its menu bar.
 */
void CVmBifBodgers::menu(VMG_ uint argc)
{
    retval_nil(vmg0_);
}

/* the front end's screen reader mode: guit3 has none */
void CVmBifBodgers::uses_accessible_mode(VMG_ uint argc)
{
    retval_nil(vmg0_);
}

/* guit3 keeps the input line in view by itself */
void CVmBifBodgers::scroll_to_bottom(VMG_ uint argc)
{
    retval_nil(vmg0_);
}

/*
 *   The front end's credits screen.  credits_patch.t replaces the method
 *   that calls this with one printing the credits.
 */
void CVmBifBodgers::show_credits(VMG_ uint argc)
{
    retval_nil(vmg0_);
}

/* Steam achievements: none here */
void CVmBifBodgers::unlock_achievement(VMG_ uint argc)
{
    pop_int_val(vmg0_);
    retval_nil(vmg0_);
}

/* the folder of patch*.t (the game's), to append file names to */
void CVmBifBodgers::patches_path(VMG_ uint argc)
{
    retval_fname(vmg_ game_dir_slash().c_str());
}

/*
 *   Where the game autosaves (new FileName(qtads_saves_path(), 'Autosave')):
 *   the game's folder, where the save dialog starts too, and which the file
 *   safety settings let the game write to.
 */
void CVmBifBodgers::saves_path(VMG_ uint argc)
{
    retval_fname(vmg_ game_dir_slash().c_str());
}

/*
 *   The platform: the game counts 100 and more as a mobile device and then
 *   leaves out the commands that need a file dialog.  guit3 has file
 *   dialogs everywhere, so it is never one.
 */
void CVmBifBodgers::platform(VMG_ uint argc)
{
    retval_int(vmg_ 0);
}

/* shown in the startup banner: "<platform> interpreter version <version>" */
void CVmBifBodgers::platform_str(VMG_ uint argc)
{
    retval_str(vmg_ "GuiT3");
}

void CVmBifBodgers::startup_check(VMG_ uint argc)
{
    retval_nil(vmg0_);
}

void CVmBifBodgers::version(VMG_ uint argc)
{
    retval_str(vmg_ HTMLTADS_VERSION " (" GUITADS_BUILD ")");
}

/* the map's state, every turn */
void CVmBifBodgers::update_map(VMG_ uint argc)
{
    const char *str = pop_str_val(vmg0_);
    guimap_update(str + VMB_LEN, vmb_get_len(str));
    retval_nil(vmg0_);
}

/* the MAP command */
void CVmBifBodgers::show_map(VMG_ uint argc)
{
    guimap_show();
    retval_nil(vmg0_);
}
