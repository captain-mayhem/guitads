/*
 *   vmbifreg_guit3.cpp - guit3's built-in function set registry
 *
 *   A copy of tads3/vmbifregxn.cpp (with the network set) or
 *   tads3/vmbifregx.cpp (without it), plus guit3's own function set
 *   "bodgers/002" (guibodgers.h), as vmbifreg.cpp's own notes describe for
 *   host applications with extra function sets.
 *
 *   This file defines G_bif_reg_table and every listed set's bif_table, so
 *   the linker never takes t3htm's copy of the registry: nothing else in
 *   that object file is used.  t3htm stays shared with htmlt3/htmltdb3,
 *   which keep the plain registry.  Keep the list in step with
 *   vmbifregxn.cpp when TADS adds a set.
 *
 *   Copyright (c) 1998, 2002 Michael J. Roberts (the registry this copies).
 */

#include "vmbifreg.h"

/*
 *   Include the function set vector definitions.
 */
#define VMBIF_DEFINE_VECTOR

#include "vmbiftad.h"
#include "vmbiftio.h"
#include "vmbift3.h"
#include "vmbiftix.h"
#ifdef TADSNET
#include "vmbifnet.h"
#endif

/* guit3's function sets */
#include "guibodgers.h"

#undef VMBIF_DEFINE_VECTOR

#define MAKE_ENTRY(entry_name, cls) \
    { entry_name, countof(cls::bif_table), cls::bif_table, \
      &cls::attach, &cls::detach }

vm_bif_entry_t G_bif_reg_table[] =
{
    /* T3 VM system function set, v1 */
    MAKE_ENTRY("t3vm/010006", CVmBifT3),

    /* T3 VM Testing interface, v1 */
    MAKE_ENTRY("t3vmTEST/010000", CVmBifT3Test),

    /* TADS generic data manipulation functions */
    MAKE_ENTRY("tads-gen/030008", CVmBifTADS),

    /* TADS input/output functions */
    MAKE_ENTRY("tads-io/030007", CVmBifTIO),

    /* TADS extended input/output functions */
    MAKE_ENTRY("tads-io-ext/030000", CVmBifTIOExt),

#ifdef TADSNET
    /* TADS network functions */
    MAKE_ENTRY("tads-net/030001", CVmBifNet),
#endif

    /* the front end of Thaumistry: In Charm's Way (guibodgers.h) */
    MAKE_ENTRY("bodgers/002", CVmBifBodgers),

    /* end of table marker */
    { 0, 0, 0 }
};

#undef MAKE_ENTRY
