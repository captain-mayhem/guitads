/*
 *   guimap.h - the map screen of Thaumistry: In Charm's Way
 *
 *   The game's front end (guibodgers.h) shows a map when the player types
 *   MAP: a map of the area the player is in, with a cloud over every room
 *   not visited yet and a dot on the current room, or (Tab, or a double tap)
 *   the world map of New York with a pin for every area reached.  The game
 *   tells the front end where the player is and what is visited every turn
 *   (qtads_update_map(), a flat JSON object: "current_section",
 *   "current_room", and true/false per room and area).
 *
 *   The pictures are TIFFs in maps.bin next to the game, a Qt resource file
 *   ("qres"), decoded with the engine's TIFF codec.  Where on the maps the
 *   rooms and areas are isn't in the files: those positions are the front
 *   end's, in the table in guimap.cpp.  See migration.md 5.37.
 */

#ifndef GUIMAP_H
#define GUIMAP_H

#include <stddef.h>

/*
 *   A game with the map was loaded from 'game_dir' (maps.bin is there); null
 *   forgets the map (the game ended).  Clears what the game told about
 *   the player.
 */
void guimap_set_game(const char *game_dir);

/* the game's map state, the JSON object of qtads_update_map() */
void guimap_update(const char *json, size_t len);

/* is there a map to show (maps.bin was found and is readable)? */
bool guimap_available();

/*
 *   Show the map until the player closes it (Esc, Back, or the close
 *   button), running its own frames like a modal dialog.  Returns false if
 *   the application is quitting (the window was closed meanwhile).
 */
bool guimap_show();

#endif /* GUIMAP_H */
