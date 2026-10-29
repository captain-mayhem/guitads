Sample fonts vendored from the upstream Dear ImGui project's own misc/fonts/
directory (https://github.com/ocornut/imgui), added here as the packaged
font set for guit3's Emscripten build - see guit3/CMakeLists.txt's
em_package(guit3fonts ...) call. The engine's system font lookup
(CGE::SystemFonts) indexes them as the page's "system fonts". Not used by
the native Windows/Linux builds, which read fonts from the real OS font
store instead (also through the engine).

Licenses, per Dear ImGui's own misc/fonts/README.txt:

  Cousine-Regular.ttf   by Steve Matteson, Apache License 2.0
  DroidSans.ttf         by Google, Apache License 2.0
  Karla-Regular.ttf     by Jonathan Pinhorn, SIL Open Font License 1.1
  ProggyClean.ttf       by Tristan Grimmer, public domain
                        (see http://www.proggyfonts.net/)
  ProggyTiny.ttf        by Tristan Grimmer, public domain
                        (see http://www.proggyfonts.net/)
  Roboto-Medium.ttf     by Google, Apache License 2.0

Fonts added to match the Windows fonts the guit3 themes ask for
(htmlpref.cpp's set_theme_defaults()); the engine's alias table maps the
Windows names to these.  Full license texts are in licenses/.

  Liberation Sans / Serif / Mono   (Regular, Bold, Italic, BoldItalic)
                        Liberation Fonts 2.1.5, SIL Open Font License 1.1
                        Metric-compatible with Arial / Times New Roman /
                        Courier New.
  DejaVu Sans           (Regular only; bold/italic dropped to save size) v2.37,
                        Bitstream Vera / DejaVu license (free, see
                        licenses/DejaVu-LICENSE.txt).  Stand-in for
                        Verdana/Tahoma (similar width, not metric-exact).
  Gelasio               (Regular, Italic) by Eben Sorkin, SIL OFL 1.1.
                        Metric-compatible with Georgia.  These are the
                        variable-font files from google/fonts; FreeType
                        loads the default (Regular, wght=400) instance, so
                        bold is not available.
  Comic Relief          (Regular, Bold) by Jeff Davis, SIL OFL 1.1.
                        Metric-compatible with Comic Sans MS.

The older Cousine/DroidSans/Karla/Proggy/Roboto files above are kept but are
no longer the substitution targets for Windows font names.
