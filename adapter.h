/*
 * Generic interface to a debug port adapter.
 *
 * Copyright (C) 2011-2013 Serge Vakulenko
 *
 * This file is part of PIC32PROG project, which is distributed
 * under the terms of the GNU General Public License (GPL).
 * See the accompanying file "COPYING" for more details.
 */

#ifndef _ADAPTER_H
#define _ADAPTER_H

#include <stdarg.h>

#define AD_READ  0x0001
#define AD_WRITE 0x0002
#define AD_ERASE 0x0004
#define AD_PROBE 0x0008

typedef struct _adapter_t adapter_t;

struct _adapter_t {
    unsigned user_start;                /* Start address of user area */
    unsigned user_nbytes;               /* Size of user flash area */
    unsigned boot_nbytes;               /* Size of user boot area */

    unsigned block_override;            /* Overridden block size for target */

    unsigned flags;
    const char *family_name;            /* Name of pic32 family */

    void (*close)(adapter_t *a, int power_on);
    unsigned (*get_idcode)(adapter_t *a);
    void (*load_executive)(adapter_t *a,
        const unsigned *pe, unsigned nwords, unsigned pe_version);
    void (*read_data)(adapter_t *a, unsigned addr, unsigned nwords, unsigned *data);
    void (*verify_data)(adapter_t *a, unsigned addr, unsigned nwords, unsigned *data);
    void (*program_block)(adapter_t *a, unsigned addr, unsigned *data);
    void (*program_quad_word)(adapter_t *a, unsigned addr, unsigned word0,
        unsigned word1, unsigned word2, unsigned word3);
    void (*program_row)(adapter_t *a, unsigned addr, unsigned *data, unsigned words_per_row);
    void (*program_word)(adapter_t *a, unsigned addr, unsigned word);
    unsigned (*read_word)(adapter_t *a, unsigned addr);
    void (*erase_chip)(adapter_t *a);
};

adapter_t *adapter_open_pickit2(int vid, int pid, const char *serial, int report);
adapter_t *adapter_open_pickit3(int vid, int pid, const char *serial, int report);
adapter_t *adapter_open_an1388(int vid, int pid, const char *serial, int report);
adapter_t *adapter_open_hidboot(int vid, int pid, const char *serial, int report);
adapter_t *adapter_open_mpsse(int vid, int pid, const char *serial, int report);
adapter_t *adapter_open_bitbang(const char *port, int baud_rate);
adapter_t *adapter_open_an1388_uart(const char *port, int baud_rate);
adapter_t *adapter_open_stk500v2(const char *port, int baud_rate);
adapter_t *adapter_open_uhb(int vid, int pid, const char *serial, int report);

void mdelay(unsigned msec);
extern int debug_level;

#endif
