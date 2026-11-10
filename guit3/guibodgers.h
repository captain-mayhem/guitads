/*
 *   guibodgers.h - the "bodgers" function set (bodgers/002)
 *
 *   The intrinsic functions of the QTads-based front end Thaumistry: In
 *   Charm's Way ships with (Thaumistry.exe, built from "qtads-bodgers").
 *   The full game (vmimage.bin, next to maps.bin and the patch*.t files)
 *   requires this function set; Thaumistry.t3 is a fallback build for other
 *   interpreters that leaves the front end out.  The functions and their
 *   arguments come from the game's own byte code and symbol table, as the
 *   front end's source isn't published - see migration.md 5.37:
 *
 *      0  DEPRECATED(...)                  -> nil
 *      1  qtads_menu()                     the front end's main menu
 *      2  qtads_uses_accessible_mode()     screen reader mode -> nil
 *      3  qtads_scroll_to_bottom()
 *      4  qtads_show_credits()             credits_patch.t prints them instead
 *      5  qtads_unlock_achievement(n)      Steam achievements
 *      6  qtads_patches_path()             folder of patch*.t, with separator
 *      7  qtads_saves_path()               where the game autosaves
 *      8  qtads_platform()                 100 or more means a mobile device
 *      9  qtads_platform_str()             shown in the startup banner
 *     10  qtads_startup_check()            -> nil
 *     11  qtads_version()                  shown in the startup banner
 *     12  qtads_update_map(json)           where the player is, what's visited
 *     13  qtads_show_map()                 the MAP command: shows guimap.h
 *
 *   It is linked through guit3's own copy of the function set registry,
 *   vmbifreg_guit3.cpp.  Like the TADS headers, this one defines the
 *   function vector when VMBIF_DEFINE_VECTOR is defined.
 */

#ifndef GUIBODGERS_H
#define GUIBODGERS_H

#include "vmbif.h"

class CVmBifBodgers: public CVmBif
{
public:
    /* function vector */
    static vm_bif_desc bif_table[];

    /* the game is loaded: remember its folder, add its fonts and theme */
    static void attach(VMG0_);

    /* the game is over: forget the map */
    static void detach(VMG0_);

    static void deprecated(VMG_ uint argc);
    static void menu(VMG_ uint argc);
    static void uses_accessible_mode(VMG_ uint argc);
    static void scroll_to_bottom(VMG_ uint argc);
    static void show_credits(VMG_ uint argc);
    static void unlock_achievement(VMG_ uint argc);
    static void patches_path(VMG_ uint argc);
    static void saves_path(VMG_ uint argc);
    static void platform(VMG_ uint argc);
    static void platform_str(VMG_ uint argc);
    static void startup_check(VMG_ uint argc);
    static void version(VMG_ uint argc);
    static void update_map(VMG_ uint argc);
    static void show_map(VMG_ uint argc);
};

#endif /* GUIBODGERS_H */

#ifdef VMBIF_DEFINE_VECTOR

vm_bif_desc CVmBifBodgers::bif_table[] =
{
    { &CVmBifBodgers::deprecated, 0, 0, TRUE },
    { &CVmBifBodgers::menu, 0, 0, FALSE },
    { &CVmBifBodgers::uses_accessible_mode, 0, 0, FALSE },
    { &CVmBifBodgers::scroll_to_bottom, 0, 0, FALSE },
    { &CVmBifBodgers::show_credits, 0, 0, FALSE },
    { &CVmBifBodgers::unlock_achievement, 1, 0, FALSE },
    { &CVmBifBodgers::patches_path, 0, 0, FALSE },
    { &CVmBifBodgers::saves_path, 0, 0, FALSE },
    { &CVmBifBodgers::platform, 0, 0, FALSE },
    { &CVmBifBodgers::platform_str, 0, 0, FALSE },
    { &CVmBifBodgers::startup_check, 0, 0, FALSE },
    { &CVmBifBodgers::version, 0, 0, FALSE },
    { &CVmBifBodgers::update_map, 1, 0, FALSE },
    { &CVmBifBodgers::show_map, 0, 0, FALSE }
};

#endif /* VMBIF_DEFINE_VECTOR */
