/*
 *   guikeys.h - guit3's canonical key codes
 *
 *   guit3's canonical key code (os_key_t, guios.h) used to be a GLFW_KEY_*
 *   value.  Since guit3 runs on Captain's Game Engine, GLFW is gone; these
 *   constants keep the same numbers under guit3's own names, so the
 *   accelerator tables, CTadsKeyboard's key names and anything stored with
 *   them stay as they were.  The printable keys are their ASCII codes
 *   (GUI_KEY_A..GUI_KEY_Z are 'A'..'Z', GUI_KEY_0..GUI_KEY_9 are '0'..'9').
 *
 *   The keys' live state comes from Dear ImGui, which the engine feeds with
 *   the window's key events: gui_key_to_imgui() names the ImGui key of a
 *   canonical key.  The engine's keys follow the keyboard layout (Windows
 *   virtual keys, X11 KeySyms, the browser's key codes), so GUI_KEY_Y is the
 *   key labeled Y, also on a German QWERTZ keyboard.
 */

#ifndef GUIKEYS_H
#define GUIKEYS_H

#include <imgui.h>

#define GUI_KEY_SPACE              32
#define GUI_KEY_APOSTROPHE         39  /* ' */
#define GUI_KEY_COMMA              44  /* , */
#define GUI_KEY_MINUS              45  /* - */
#define GUI_KEY_PERIOD             46  /* . */
#define GUI_KEY_SLASH              47  /* / */
#define GUI_KEY_0                  48
#define GUI_KEY_9                  57
#define GUI_KEY_SEMICOLON          59  /* ; */
#define GUI_KEY_EQUAL              61  /* = */
#define GUI_KEY_A                  65
#define GUI_KEY_C                  67
#define GUI_KEY_F                  70
#define GUI_KEY_O                  79
#define GUI_KEY_Q                  81
#define GUI_KEY_R                  82
#define GUI_KEY_S                  83
#define GUI_KEY_V                  86
#define GUI_KEY_X                  88
#define GUI_KEY_Y                  89
#define GUI_KEY_Z                  90
#define GUI_KEY_LEFT_BRACKET       91  /* [ */
#define GUI_KEY_BACKSLASH          92  /* \ */
#define GUI_KEY_RIGHT_BRACKET      93  /* ] */
#define GUI_KEY_GRAVE_ACCENT       96  /* ` */

#define GUI_KEY_ESCAPE             256
#define GUI_KEY_ENTER              257
#define GUI_KEY_TAB                258
#define GUI_KEY_BACKSPACE          259
#define GUI_KEY_INSERT             260
#define GUI_KEY_DELETE             261
#define GUI_KEY_RIGHT              262
#define GUI_KEY_LEFT               263
#define GUI_KEY_DOWN               264
#define GUI_KEY_UP                 265
#define GUI_KEY_PAGE_UP            266
#define GUI_KEY_PAGE_DOWN          267
#define GUI_KEY_HOME               268
#define GUI_KEY_END                269
#define GUI_KEY_PRINT_SCREEN       283
#define GUI_KEY_PAUSE              284
#define GUI_KEY_F1                 290
#define GUI_KEY_F2                 291
#define GUI_KEY_F3                 292
#define GUI_KEY_F4                 293
#define GUI_KEY_F5                 294
#define GUI_KEY_F6                 295
#define GUI_KEY_F7                 296
#define GUI_KEY_F8                 297
#define GUI_KEY_F9                 298
#define GUI_KEY_F10                299
#define GUI_KEY_F11                300
#define GUI_KEY_F12                301
#define GUI_KEY_F13                302
#define GUI_KEY_F14                303
#define GUI_KEY_F15                304
#define GUI_KEY_F16                305
#define GUI_KEY_F17                306
#define GUI_KEY_F18                307
#define GUI_KEY_F19                308
#define GUI_KEY_F20                309
#define GUI_KEY_F21                310
#define GUI_KEY_F22                311
#define GUI_KEY_F23                312
#define GUI_KEY_F24                313
#define GUI_KEY_KP_0               320
#define GUI_KEY_KP_1               321
#define GUI_KEY_KP_2               322
#define GUI_KEY_KP_3               323
#define GUI_KEY_KP_4               324
#define GUI_KEY_KP_5               325
#define GUI_KEY_KP_6               326
#define GUI_KEY_KP_7               327
#define GUI_KEY_KP_8               328
#define GUI_KEY_KP_9               329
#define GUI_KEY_KP_DECIMAL         330
#define GUI_KEY_KP_DIVIDE          331
#define GUI_KEY_KP_MULTIPLY        332
#define GUI_KEY_KP_SUBTRACT        333
#define GUI_KEY_KP_ADD             334

/* the highest canonical key code (GLFW_KEY_MENU, 348) */
#define GUI_KEY_LAST               348

/* the ImGui key of a canonical key code, ImGuiKey_None if there is none */
ImGuiKey gui_key_to_imgui(int key);

#endif /* GUIKEYS_H */
