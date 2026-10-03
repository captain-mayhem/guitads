# guitads

guitads holds `guit3`, a cross-platform (Dear ImGui / GLFW) HTML TADS 3
interpreter ported from the Win32 `htmlt3` client of HTML TADS. It also holds
the HTML TADS core and the third-party libraries guit3 needs. It is not built
on its own: tads-runner picks it up as a sibling checkout via
`add_subdirectory()`.

## Licensing: an honest assessment

> This is an engineering assessment, not legal advice. It is based on a line-by-line comparison of
> every guit3 file against the original HTML TADS import (htmltads commit
> `b1a60cf`).

**guit3 is not GPL software today.** The GPLv3 `LICENSE` file does not reflect
what is actually in `guit3/`.

- Only about **10% of guit3** (≈7,200 of ≈75,000 lines) is newly written code.
  The rest is Michael J. Roberts' Win32 client code, adapted to varying
  degrees. That code is covered by the **HTML TADS Freeware Source Code
  License**.
- **Freeware path.** The HTML TADS license explicitly permits *ports to new
  platforms*, and guit3 is one. Distribute guit3 under that license: free of
  charge, with the license text and your contact details included.

## Which license covers which file

**GPLv3 (via QTads)**: these `htmltads/` files are unchanged from QTads apart
from guitads' own edits:

`html_os.h`, `htmlattr.h`, `htmldisp.*`, `htmlfmt.*`, `htmlhash.*`,
`htmlinp.*`, `htmlprs.*`, `htmlrc.*`, `htmlreg.h`, `htmlrf.*`, `htmlsnd.*`,
`htmlsys.*`, `htmltags.*`, `htmltxar.*`, `htmlurl.h`, `htmlver.h`, `oshtml.cpp`,
`tadshtml.*`, `tadsrtyp.cpp`

**New code in `guit3/`** (the author's own; GPLv3 or any license of the
author's choice). These files share at most incidental lines with the
originals:

`tadsplat.h`, `guios.h`, `guios_common.cpp`, `guios_portable.cpp`,
`tadsfiledlg.*`, `tadsfinddlg.*`, `tadsfolderdlg.*`, `tadslicensedlg.*`,
`tadssettings.h`, `tadssettings_portable.cpp`, `tadsaudiodev.*`, `fcfont.cpp`,
`ctfont.cpp`, `emfont.cpp`, `emscripten/*`, `CMakeLists.txt`, `*.md`

**Still under the HTML TADS freeware license**

- `htmltads/`: `hos_gui.h` (derived from `hos_w32.h`), `htmldbg.h`,
  `htmljpeg.*`, `htmlmng.*`, `htmlplst.*`, `htmlpng.*`, `pngext.*`,
  `tadshtml3.cpp`, `win32/hos_w32.h`. These came from the htmltads repo, not
  from QTads.
- `mpegamp/`: amp (© Tomislav Uzelac, amp license) with Roberts' changes.
- `guit3/`, essentially unmodified (≈85–100% original code):
  `foldsel.h`, `foldsel2.cpp`, `foldselr.h`, `htmlres.h`, `res2.h`,
  `resource.h`, `oem_w32.c`, `t3main.*`, `tadsapp.*`, `tadscar.*`,
  `tadschest.cpp`, `tadscom.*`, `tadsdlg.*`, `tadsdlg2.cpp`, `tadsistr.h`,
  `tadsjpeg.*`, `tadsmidi.*`, `tadsmng.*`, `tadsole.*`, `tadspng.h`,
  `tadstab.*`, `tadsvorb.h`, `tadswebctl.*`, `guiimg.*`, `guimain.h`,
  `guinogch.cpp`, `guisnd.*`, `guifont.h`, `guiwebui.h`, `htmlgui.h`,
  `mpegamp_w32.cpp`, `getbits.cpp`
- `guit3/`, substantially modified but still clearly derived:
  `htmlgui.cpp`, `hos_gui.cpp`, `tadswin.*`, `guimain.cpp`, `htmlpref.*`,
  `guit3.cpp`, `guitr.cpp`, `guitrt3.cpp`, `guiver.h`, `mpegamp_w32.h`,
  `tadskb.*`, `tadssnd.*`, `tadsstat.*`, `tadswav.*`, `tadsvorb.cpp`,
  `tadsimg.*`, `tadsfont.*`, `tadspng.cpp`, `tadscsnd.*`, `guifont.cpp`,
  `guifont_w32.cpp`, `tadssettings_w32.cpp`
- `guit3/`, assets: `about3.jpg`, and `guires_data.*` (embeds
  `runtbar.bmp` and the license text)
- `guit3/`, grey zone: `guios_w32.cpp`. It has only ~9% verbatim overlap, but
  by its own description it is Win32 call-site code moved unchanged. It is
  small enough to rewrite.

**Third-party libraries**: zlib, libpng, IJG jpeg, libmng, GLFW, FreeType,
Dear ImGui, libogg/libvorbis and miniaudio. All are GPLv3-compatible.
