/*
 * Localization using gettext.
 *
 * Copyright (C) 2010-2013 Serge Vakulenko
 *
 * This file is part of PIC32PROG project, which is distributed
 * under the terms of the GNU General Public License (GPL).
 * See the accompanying file "COPYING" for more details.
 */
#if 1
    /* No localization. */
    #define _(str)                      (str)
    #define N_(str)                     str
    #define textdomain(name)            /* empty */
    #define bindtextdomain(name,dir)    /* empty */
#else
    /* Use gettext(). */
    #include <libintl.h>
    #define _(str)                      gettext (str)
    #define gettext_noop(str)           str
    #define N_(str)                     gettext_noop (str)
#endif
