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
#include <stdint.h> 

typedef void print_func_t(unsigned cfg0, unsigned cfg1, unsigned cfg2, unsigned cfg3,
						unsigned cfg4, unsigned cfg5, unsigned cfg6, unsigned cfg7,
						unsigned cfg8, unsigned cfg9, unsigned cfg10, unsigned cfg11,
                        unsigned cfg12, unsigned cfg13, unsigned cfg14, unsigned cfg15,
                        unsigned cfg16, unsigned cfg17);

typedef struct {
    const char      *name;
	unsigned		name_short;
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

target_t *target_open(const char *port, int baud_rate, int interface, int speed);
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
void target_program_devcfg(target_t *t, uint32_t arg0, uint32_t arg1,
        uint32_t arg2, uint32_t arg3, uint32_t arg4, uint32_t arg5, 
        uint32_t arg6, uint32_t arg7, uint32_t arg8, uint32_t arg9, 
        uint32_t arg10, uint32_t arg11, uint32_t arg12, uint32_t arg13);

#endif
