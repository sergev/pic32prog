/*
 * Generic debug interface to a target microcontroller.
 *
 * Copyright (C) 2011-2013 Serge Vakulenko
 *
 * This file is part of PIC32PROG project, which is distributed
 * under the terms of the GNU General Public License (GPL).
 * See the accompanying file "COPYING" for more details.
 */

#ifndef _TARGET_H
#define _TARGET_H

#include "adapter.h"

typedef void print_func_t(unsigned cfg0, unsigned cfg1, unsigned cfg2, unsigned cfg3);

typedef struct {
    const char      *name;
    unsigned        boot_kbytes;
    unsigned        devcfg_offset;
    unsigned        bytes_per_row;
    print_func_t    *print_devcfg;
    const unsigned  *pe_code;
    unsigned        pe_nwords;
    unsigned        pe_version;
} family_t;

typedef struct {
    unsigned        devid;
    const char      *name;
    unsigned        flash_kbytes;
    const family_t  *family;
} variant_t;

typedef struct {
    adapter_t       *adapter;
    const char      *cpu_name;
    const family_t  *family;
    unsigned        cpuid;
    unsigned        flash_addr;
    unsigned        flash_bytes;
    unsigned        boot_bytes;
} target_t;

target_t *target_open(const char *port, int baud_rate);
void target_close(target_t *t, int power_on);
void target_use_executive(target_t *t);
void target_configure(void);
void target_add_variant(char *name, unsigned id, char *family, unsigned flash_kbytes);

unsigned target_idcode(target_t *t);
const char *target_cpu_name(target_t *t);
unsigned target_flash_width(target_t *t);
unsigned target_flash_bytes(target_t *t);
unsigned target_boot_bytes(target_t *t);
unsigned target_block_size(target_t *t);
unsigned target_devcfg_offset(target_t *t);
void target_print_devcfg(target_t *t);

void target_read_block(target_t *t, unsigned addr,
	unsigned nwords, unsigned *data);
void target_verify_block(target_t *t, unsigned addr,
	unsigned nwords, unsigned *data);

int target_erase(target_t *t);
void target_program_block(target_t *t, unsigned addr,
	unsigned nwords, unsigned *data);
void target_program_devcfg(target_t *t, unsigned devcfg0,
        unsigned devcfg1, unsigned devcfg2, unsigned devcfg3);

#endif
