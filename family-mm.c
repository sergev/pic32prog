/*
 * Routines specific for PIC32 MZ family.
 *
 * Copyright (C) 2013 Serge Vakulenko
 *
 * This file is part of PIC32PROG project, which is distributed
 * under the terms of the GNU General Public License (GPL).
 * See the accompanying file "COPYING" for more details.
 */
#include <stdio.h>
#include "pic32.h"
#include "target.h"

/*
 * Print configuration for MM family.
 */
void print_mm(target_t *t, unsigned cfg0, unsigned cfg1, unsigned cfg2, unsigned cfg3)
{
    unsigned fdevopt = t->adapter->read_word(t->adapter, 0x1fc017C4);
    unsigned ficd    = t->adapter->read_word(t->adapter, 0x1fc017C8);
    unsigned fpor    = t->adapter->read_word(t->adapter, 0x1fc017CC);
    unsigned fwdt    = t->adapter->read_word(t->adapter, 0x1fc017D0);
    unsigned foscsel = t->adapter->read_word(t->adapter, 0x1fc017D4);
    unsigned fsec    = t->adapter->read_word(t->adapter, 0x1fc017D8);
    unsigned devid   = t->adapter->read_word(t->adapter, 0x1f803B20);

    printf("    DEVID   = %08x\n", devid);
    printf("    FDEVOPT = %08x\n", fdevopt);
    printf("    FICD    = %08x\n", ficd);
    printf("    FPOR    = %08x\n", fpor);
    printf("    FWDT    = %08x\n", fwdt);
    printf("    FOSCSEL = %08x\n", foscsel);
    printf("    FSEC    = %08x\n", fsec);
}
