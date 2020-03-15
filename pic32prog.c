/*
 * Flash memory programmer for Microchip PIC32 microcontrollers.
 *
 * Copyright (C) 2011-2014 Serge Vakulenko
 *
 * This file is part of PIC32PROG project, which is distributed
 * under the terms of the GNU General Public License (GPL).
 * See the accompanying file "COPYING" for more details.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <signal.h>
#include <getopt.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <time.h>
#include <libgen.h>
#include <locale.h>

#include "target.h"
#include "serial.h"
#include "localize.h"
#include "adapter.h"

#include "pic32.h"

#ifndef VERSION
#define VERSION         "2.0."GITCOUNT
#endif
#define MINBLOCKSZ          128
#define FLASHV_KSEG0_BASE   0x9d000000
#define BOOTV_KSEG0_BASE    0x9fc00000
#define FLASHV_KSEG1_BASE   0xBD000000
#define BOOTV_KSEG1_BASE    0xBFC00000
#define FLASHP_BASE     0x1d000000
#define BOOTP_BASE      0x1fc00000
#define FLASH_BYTES     (2048 * 1024)
#define BOOT_BYTES      (512 * 1024)	// Fix for MK family, space is 404kB big

/* Macros for converting between hex and binary. */
#define NIBBLE(x)       (isdigit(x) ? (x)-'0' : tolower(x)+10-'a')
#define HEX(buffer)     ((NIBBLE((buffer)[0])<<4) + NIBBLE((buffer)[1]))

/* Data to write */
unsigned char boot_data [BOOT_BYTES];
unsigned char flash_data [FLASH_BYTES];
unsigned char boot_dirty [BOOT_BYTES / MINBLOCKSZ];
unsigned char flash_dirty [FLASH_BYTES / MINBLOCKSZ];
unsigned blocksz;               /* Size of flash memory block */
unsigned boot_used;
unsigned char bootv_kseg = 1;    // Default to 1, same as before. Set in store_data.
unsigned char flashv_kseg = 1;   // Default to 1, same as before. Set in store_data.
unsigned flash_used;
unsigned boot_bytes;
unsigned flash_bytes;
unsigned devcfg_offset;         /* Offset of devcfg registers in boot data */
int total_bytes;
int interface = INTERFACE_DEFAULT;  /* Optionally specified JTAG or ICSP */
int interface_speed = 0;            /* Optional clock speed of interface */

// PIC32MX, MZ DEVCFG definitions
#define devcfg3 (*(unsigned*) &boot_data [devcfg_offset])
#define devcfg2 (*(unsigned*) &boot_data [devcfg_offset + 4])
#define devcfg1 (*(unsigned*) &boot_data [devcfg_offset + 8])
#define devcfg0 (*(unsigned*) &boot_data [devcfg_offset + 12])

// PIC32MK DEVCFG definitions
#define bf1devcfg3 	(*(unsigned*) &boot_data [devcfg_offset + 0x40000])
#define bf1devcfg2 	(*(unsigned*) &boot_data [devcfg_offset + 0x40000 + 4])
#define bf1devcfg1 	(*(unsigned*) &boot_data [devcfg_offset + 0x40000 + 8])
#define bf1devcfg0 	(*(unsigned*) &boot_data [devcfg_offset + 0x40000 + 12])
#define bf1devcp 	(*(unsigned*) &boot_data [devcfg_offset + 0x40000 + 28])
#define bf1devsign 	(*(unsigned*) &boot_data [devcfg_offset + 0x40000 + 44])
#define bf1seq 		(*(unsigned*) &boot_data [devcfg_offset + 0x40000 + 48])

#define bf2devcfg3 	(*(unsigned*) &boot_data [devcfg_offset + 0x40000 + 0x20000])
#define bf2devcfg2 	(*(unsigned*) &boot_data [devcfg_offset + 0x40000 + 0x20000 + 4])
#define bf2devcfg1 	(*(unsigned*) &boot_data [devcfg_offset + 0x40000 + 0x20000 + 8])
#define bf2devcfg0 	(*(unsigned*) &boot_data [devcfg_offset + 0x40000 + 0x20000 + 12])
#define bf2devcp 	(*(unsigned*) &boot_data [devcfg_offset + 0x40000 + 0x20000 + 28])
#define bf2devsign 	(*(unsigned*) &boot_data [devcfg_offset + 0x40000 + 0x20000 + 44])
#define bf2seq 		(*(unsigned*) &boot_data [devcfg_offset + 0x40000 + 0x20000 + 48])



// PIC32MM definitions
#define offset_first 0xc0
#define offset_alternate 0x40
#define fdevopt  (*(unsigned*) &boot_data [devcfg_offset + offset_first + 0x04])
#define ficd     (*(unsigned*) &boot_data [devcfg_offset + offset_first + 0x08])
#define fpor     (*(unsigned*) &boot_data [devcfg_offset + offset_first + 0x0c])
#define fwdt     (*(unsigned*) &boot_data [devcfg_offset + offset_first + 0x10])
#define foscsel  (*(unsigned*) &boot_data [devcfg_offset + offset_first + 0x14])
#define fsec     (*(unsigned*) &boot_data [devcfg_offset + offset_first + 0x18])
#define afdevopt  (*(unsigned*) &boot_data [devcfg_offset + offset_alternate + 0x04])
#define aficd     (*(unsigned*) &boot_data [devcfg_offset + offset_alternate + 0x08])
#define afpor     (*(unsigned*) &boot_data [devcfg_offset + offset_alternate + 0x0c])
#define afwdt     (*(unsigned*) &boot_data [devcfg_offset + offset_alternate + 0x10])
#define afoscsel  (*(unsigned*) &boot_data [devcfg_offset + offset_alternate + 0x14])
#define afsec     (*(unsigned*) &boot_data [devcfg_offset + offset_alternate + 0x18])

unsigned progress_count;
int verify_only;
int erase_only = 0;
int skip_verify = 0;
int debug_level;
int power_on;
target_t *target;
const char *target_port;        /* Optional name of target serial or USB port */
int target_speed = 115200;      /* Baud rate for serial port */
int alternate_speed = 115200;   /* Alternate speed for serial port */
char *progname;
const char *copyright;

void *fix_time()
{
    static struct timeval t0;

    gettimeofday(&t0, 0);
    return &t0;
}

unsigned mseconds_elapsed(void *arg)
{
    struct timeval t1, *t0 = arg;
    unsigned mseconds;

    gettimeofday(&t1, 0);
    mseconds = (t1.tv_sec - t0->tv_sec) * 1000 +
        (t1.tv_usec - t0->tv_usec) / 1000;
    if (mseconds < 1)
        mseconds = 1;
    return mseconds;
}

void store_data(unsigned address, unsigned byte)
{
    unsigned offset;

    if (address >= BOOTV_KSEG0_BASE && address < BOOTV_KSEG0_BASE + BOOT_BYTES) {
        /* Boot code, virtual. KSEG0! */
        offset = address - BOOTV_KSEG0_BASE;
        boot_data [offset] = byte;
        boot_used = 1;
        bootv_kseg = 0;
    } else if (address >= BOOTV_KSEG1_BASE && address < BOOTV_KSEG1_BASE + BOOT_BYTES) {
        /* Boot code, virtual. KSEG1! */
        offset = address - BOOTV_KSEG1_BASE;
        boot_data [offset] = byte;
        boot_used = 1;
        bootv_kseg = 1;
    } else if (address >= BOOTP_BASE && address < BOOTP_BASE + BOOT_BYTES) {
        /* Boot code, physical. */
        offset = address - BOOTP_BASE;
        boot_data [offset] = byte;
        boot_used = 1;
    } else if (address >= FLASHV_KSEG1_BASE && address < FLASHV_KSEG1_BASE + FLASH_BYTES) {
        /* Main flash memory, virtual. */
        offset = address - FLASHV_KSEG1_BASE;
        flash_data [offset] = byte;
        flash_used = 1;
        flashv_kseg = 1;
    }
    else if (address >= FLASHV_KSEG0_BASE && address < FLASHV_KSEG0_BASE + FLASH_BYTES) {
        /* Main flash memory, virtual. */
        offset = address - FLASHV_KSEG0_BASE;
        flash_data [offset] = byte;
        flash_used = 1;
        flashv_kseg = 0;
    } else if (address >= FLASHP_BASE && address < FLASHP_BASE + FLASH_BYTES) {
        /* Main flash memory, physical. */
        offset = address - FLASHP_BASE;
        flash_data [offset] = byte;
        flash_used = 1;
    } else {
        /* Ignore incorrect data. */
        //fprintf(stderr, _("%08X: address out of flash memory\n"), address);
		fprintf(stdout, "Else statement\n");
        return;
    }
    total_bytes++;
}

/*
 * Read the S record file.
 */
int read_srec(char *filename)
{
    FILE *fd;
    unsigned char buf [256];
    unsigned char *data;
    unsigned address;
    int bytes;

    fd = fopen(filename, "r");
    if (! fd) {
        perror(filename);
        exit(1);
    }

    while (fgets((char*) buf, sizeof(buf), fd)) {
        if (buf[0] == '\n')
            continue;
        if (buf[0] != 'S') {
            fclose(fd);
            return 0;
        }
        if (buf[1] == '7' || buf[1] == '8' || buf[1] == '9')
            break;

        /* Starting an S-record.  */
        if (! isxdigit(buf[2]) || ! isxdigit(buf[3])) {
            fprintf(stderr, _("%s: bad SREC record: %s\n"), filename, buf);
            exit(1);
        }
        bytes = HEX(buf + 2);

        /* Ignore the checksum byte.  */
        --bytes;

        address = 0;
        data = buf + 4;
        switch (buf[1]) {
        case '3':
            address = HEX(data);
            data += 2;
            --bytes;
            /* Fall through.  */
        case '2':
            address = (address << 8) | HEX(data);
            data += 2;
            --bytes;
            /* Fall through.  */
        case '1':
            address = (address << 8) | HEX(data);
            data += 2;
            address = (address << 8) | HEX(data);
            data += 2;
            bytes -= 2;

            while (bytes-- > 0) {
                store_data(address++, HEX(data));
                data += 2;
            }
            break;
        }
    }
    fclose(fd);
    return 1;
}

/*
 * Read HEX file.
 */
int read_hex(char *filename)
{
    FILE *fd;
    unsigned char buf [256], data[16], record_type, sum;
    unsigned address, high;
    int bytes, i;

    fd = fopen(filename, "r");
    if (! fd) {
        perror(filename);
        exit(1);
    }
    high = 0;
    while (fgets((char*) buf, sizeof(buf), fd)) {
        if (buf[0] == '\n')
            continue;
        if (buf[0] != ':') {
            fclose(fd);
            return 0;
        }
        if (! isxdigit(buf[1]) || ! isxdigit(buf[2]) ||
            ! isxdigit(buf[3]) || ! isxdigit(buf[4]) ||
            ! isxdigit(buf[5]) || ! isxdigit(buf[6]) ||
            ! isxdigit(buf[7]) || ! isxdigit(buf[8])) {
            fprintf(stderr, _("%s: bad HEX record: %s\n"), filename, buf);
            exit(1);
        }
        record_type = HEX(buf+7);
        if (record_type == 1) {
            /* End of file. */
            break;
        }
        if (record_type == 5) {
            /* Start address, ignore. */
            continue;
        }

        bytes = HEX(buf+1);
        if (strlen((char*) buf) < bytes * 2 + 11) {
            fprintf(stderr, _("%s: too short hex line\n"), filename);
            exit(1);
        }
        address = high << 16 | HEX(buf+3) << 8 | HEX(buf+5);

        sum = 0;
        for (i=0; i<bytes; ++i) {
            data [i] = HEX(buf+9 + i + i);
            sum += data [i];
        }
        sum += record_type + bytes + (address & 0xff) + (address >> 8 & 0xff);
        if (sum != (unsigned char) - HEX(buf+9 + bytes + bytes)) {
            fprintf(stderr, _("%s: bad HEX checksum\n"), filename);
            exit(1);
        }

        if (record_type == 4) {
            /* Extended address. */
            if (bytes != 2) {
                fprintf(stderr, _("%s: invalid HEX linear address record length\n"),
                    filename);
                exit(1);
            }
            high = data[0] << 8 | data[1];
            continue;
        }
        if (record_type != 0) {
            fprintf(stderr, _("%s: unknown HEX record type: %d\n"),
                filename, record_type);
            exit(1);
        }
        //printf("%08x: %u bytes\n", address, bytes);
        for (i=0; i<bytes; i++) {
            store_data(address++, data [i]);
        }
    }
    fclose(fd);
    return 1;
}

void print_symbols(char symbol, int cnt)
{
    while (cnt-- > 0)
        putchar(symbol);
}

void progress(unsigned step)
{
    ++progress_count;
    if (progress_count % step == 0) {
        putchar('#');
        fflush(stdout);
    }
}

void quit(void)
{
    if (target != 0) {
        target_close(target, power_on);
        free(target);
        target = 0;
    }
}

void interrupted(int signum)
{
    fprintf(stderr, _("\nInterrupted.\n"));
    quit();
    _exit(-1);
}

/*
 * Check that the boot block, containing devcfg registers,
 * has some useful data.
 */
static int is_flash_block_dirty(unsigned offset)
{
    int i;

    for (i=0; i<blocksz; i++, offset++) {
        if (flash_data [offset] != 0xff)
            return 1;
    }
    return 0;
}

/*
 * Check that the boot block, containing devcfg registers,
 * has some other data.
 */
static int is_boot_block_dirty(unsigned offset)
{
    int i;

    for (i=0; i<blocksz; i++, offset++) {
        /* Skip devcfg registers. */
		if (offset >= devcfg_offset && offset < devcfg_offset+16)
            continue;
        if (boot_data [offset] != 0xff)
            return 1;
    }
    return 0;
}

void do_probe()
{
    /* Open and detect the device. */
    atexit(quit);
    target = target_open(target_port, target_speed, interface, interface_speed);
    if (! target) {
        fprintf(stderr, _("Error detecting device -- check cable!\n"));
        exit(1);
    }

    if ((target->adapter->flags & AD_PROBE) == 0) {
        fprintf(stderr, _("Error: Target probe not supported.\n"));
        exit(1);
    }

    boot_bytes = target_boot_bytes(target);
    printf(_("    Processor: %s (id %08X)\n"), target_cpu_name(target),
        target_idcode(target));
    printf(_(" Flash memory: %d kbytes\n"), target_flash_bytes(target) / 1024);
    if (boot_bytes > 0)
        printf(_("  Boot memory: %d kbytes\n"), boot_bytes / 1024);
    target_print_devcfg(target);
}

/*
 * Write flash memory.
 */
void program_block(target_t *mc, unsigned addr)
{
    unsigned char *data;
    unsigned offset;

    if (addr >= BOOTV_KSEG0_BASE && addr < BOOTV_KSEG0_BASE + boot_bytes) {
        data = boot_data;
        offset = addr - BOOTV_KSEG0_BASE;
    } else if (addr >= BOOTV_KSEG1_BASE && addr < BOOTV_KSEG1_BASE + boot_bytes) {
        data = boot_data;
        offset = addr - BOOTV_KSEG1_BASE;
    } else if (addr >= BOOTP_BASE && addr < BOOTP_BASE + boot_bytes) {
        data = boot_data;
        offset = addr - BOOTP_BASE;
    } else if (addr >= FLASHV_KSEG0_BASE && addr < FLASHV_KSEG0_BASE + flash_bytes) {
        data = flash_data;
        offset = addr - FLASHV_KSEG0_BASE;
    } else if (addr >= FLASHV_KSEG1_BASE && addr < FLASHV_KSEG1_BASE + flash_bytes) {
        data = flash_data;
        offset = addr - FLASHV_KSEG1_BASE;
    } else {
        data = flash_data;
        offset = addr - FLASHP_BASE;
    }
    target_program_block(mc, addr, blocksz/4, (unsigned*) (data + offset));
}

int verify_block(target_t *mc, unsigned addr)
{
    unsigned char *data;
    unsigned offset;

    if (addr >= BOOTV_KSEG0_BASE && addr < BOOTV_KSEG0_BASE + boot_bytes) {
        data = boot_data;
        offset = addr - BOOTV_KSEG0_BASE;
    } if (addr >= BOOTV_KSEG1_BASE && addr < BOOTV_KSEG1_BASE + boot_bytes) {
        data = boot_data;
        offset = addr - BOOTV_KSEG1_BASE;
    } else if (addr >= BOOTP_BASE && addr < BOOTP_BASE + boot_bytes) {
        data = boot_data;
        offset = addr - BOOTP_BASE;
    } else if (addr >= FLASHV_KSEG0_BASE && addr < FLASHV_KSEG0_BASE + flash_bytes) {
        data = flash_data;
        offset = addr - FLASHV_KSEG0_BASE;
    } else if (addr >= FLASHV_KSEG1_BASE && addr < FLASHV_KSEG1_BASE + flash_bytes) {
        data = flash_data;
        offset = addr - FLASHV_KSEG1_BASE;
    } else {
        data = flash_data;
        offset = addr - FLASHP_BASE;
    }
    target_verify_block(mc, addr, blocksz/4, (unsigned*) (data + offset));
    return 1;
}

void do_erase()
{
    atexit(quit);
    target = target_open(target_port, target_speed, interface, interface_speed);
    if (! target) {
        fprintf(stderr, _("Error detecting device -- check cable!\n"));
        exit(1);
    }

    if ((target->adapter->flags & AD_ERASE) == 0) {
        fprintf(stderr, _("Error: Target erase not supported.\n"));
        exit(1);
    }

    target_erase(target);
}

void do_program(char *filename)
{
    unsigned addr;
    int progress_len, progress_step, boot_progress_len;
    void *t0;

    /* Open and detect the device. */
    atexit(quit);
    target = target_open(target_port, target_speed, interface, interface_speed);
    if (! target) {
        fprintf(stderr, _("Error detecting device -- check cable!\n"));
        exit(1);
    }

    if ((target->adapter->flags & AD_WRITE) == 0) {
        fprintf(stderr, _("Error: Target write not supported.\n"));
        exit(1);
    }

    flash_bytes = target_flash_bytes(target);
    boot_bytes = target_boot_bytes(target);
    if (target->adapter->block_override != 0) {
        blocksz = target->adapter->block_override;
    } else {
        blocksz = target_block_size(target);
    }
    devcfg_offset = target_devcfg_offset(target);
    printf(_("    Processor: %s\n"), target_cpu_name(target));
    printf(_(" Flash memory: %d kbytes\n"), flash_bytes / 1024);
    if (boot_bytes > 0)
        printf(_("  Boot memory: %d kbytes\n"), boot_bytes / 1024);
    printf(_("         Data: %d bytes\n"), total_bytes);

    /* Verify DEVCFGx values. */
    if (boot_used) {
        if (FAMILY_MM == target->family->name_short){
            /* Check if both values have something in them.
             * DEVOPT (and other) have some permanent 1 bits. Use those.
               It would be more sensible, to set some bits to 0 in read_*, and compare to that... */
			
            if ( ((fdevopt&0x0f00) != 0x0f00) || ((afdevopt&0x0f00) != 0x0f00)){
                fprintf(stderr, _("Configuration bits are missing -- check your HEX file!\n"));
                if (debug_level > 0){
                    fprintf(stderr, "Read config bits are:\n");
                    fprintf(stderr, "Fdevopt:  %08x\n", fdevopt);
                    fprintf(stderr, "Ficd:     %08x\n", ficd);
                    fprintf(stderr, "Fpor:     %08x\n", fpor);
                    fprintf(stderr, "Fwdt:     %08x\n", fwdt);
                    fprintf(stderr, "Foscsel:  %08x\n", foscsel);
                    fprintf(stderr, "Fsec:     %08x\n", fsec);
                    fprintf(stderr, "AFdevopt: %08x\n", afdevopt);
                    fprintf(stderr, "AFicd:    %08x\n", aficd);
                    fprintf(stderr, "AFpor:    %08x\n", afpor);
                    fprintf(stderr, "AFwdt:    %08x\n", afwdt);
                    fprintf(stderr, "AFoscsel: %08x\n", afoscsel);
                    fprintf(stderr, "AFsec:    %08x\n", afsec);
                }
                exit(1);
            }
        }
		else if (FAMILY_MK == target->family->name_short){
			/* Check if some bits were set to high */
            if ( (bf1devcfg0 & 0x0F000000) != 0x0F000000 ){
                fprintf(stderr, _("Configuration bits are missing -- check your HEX file!\n"));
                exit(1);
            }

			
		    // This is a bit of a hack, but OK.
		    // Any unused data should be 0xFFFFFFFF in the flash and here. But some isn't (special values, etc).
		    // For example, there's a clash in the Lower Alias Boot region. 
		    // CRC here calculates based on 0xFFFFFFFF, but GET_CRC calculates based on real data.

			// Also, because even though the registers exist, but MPLAB doesn't do anything with it...
			bf1devsign &= 0x7FFFFFFF;
			bf2devsign &= 0x7FFFFFFF;

		    uint32_t copyFrom = 0x1fc43fc0 - BOOTP_BASE;
			uint32_t copyTo = 0x1fc03fc0 - BOOTP_BASE;
			uint32_t length = 0x40;
			uint32_t counter = 0;
			for(counter = 0; counter < length; counter++){
				boot_data[copyTo + counter] = boot_data[copyFrom + counter];
			}


		}
        else{
            if (devcfg0 == 0xffffffff) {
                fprintf(stderr, _("DEVCFG values are missing -- check your HEX file!\n"));
                exit(1);
            }
            if (devcfg_offset == 0xffc0) {
                /* For MZ family, clear bits DEVSIGN0[31] and ADEVSIGN0[31]. */
                boot_data[0xFFEF] &= 0x7f;
                boot_data[0xFF6F] &= 0x7f;
            }
        }
    }

    if (! verify_only) {
        /* Erase flash. */
        target_erase(target);
    }
    target_use_executive(target);

    /* Compute dirty bits for every block. */
    if (flash_used) {
        for (addr=0; addr<flash_bytes; addr+=blocksz) {
            flash_dirty [addr / blocksz] = is_flash_block_dirty(addr);
        }
    }
    if (boot_used) {
        for (addr=0; addr<boot_bytes; addr+=blocksz) {
            boot_dirty [addr / blocksz] = is_boot_block_dirty(addr);
        }
    }

    /* Compute length of progress indicator for flash memory. */
    for (progress_step=1; ; progress_step<<=1) {
        progress_len = 0;
        for (addr=0; addr<flash_bytes; addr+=blocksz) {
            if (flash_dirty [addr / blocksz])
                progress_len++;
        }
        if (progress_len / progress_step < 64) {
            progress_len /= progress_step;
            if (progress_len < 1)
                progress_len = 1;
            break;
        }
    }

    /* Compute length of progress indicator for boot memory. */
    boot_progress_len = 1;
    for (addr=0; addr<boot_bytes; addr+=blocksz) {
        if (boot_dirty [addr / blocksz])
            boot_progress_len++;
    }

    progress_count = 0;
    t0 = fix_time();
    if (! verify_only) {
        if (flash_used) {
            printf(_("Program flash: "));
            print_symbols('.', progress_len);
            print_symbols('\b', progress_len);
            fflush(stdout);
            for (addr=0; addr<flash_bytes; addr+=blocksz) {
                if (flash_dirty [addr / blocksz]) {
                    program_block(target, addr + (flashv_kseg ? FLASHV_KSEG1_BASE : FLASHV_KSEG0_BASE));
                    progress(progress_step);
                }
            }
            printf(_("# done\n"));
        }
        if (boot_used) {
            printf(_(" Program boot: "));
            print_symbols('.', boot_progress_len);
            print_symbols('\b', boot_progress_len);
            fflush(stdout);
            for (addr=0; addr<boot_bytes; addr+=blocksz) {
                if (boot_dirty [addr / blocksz]) {
                    program_block(target, addr + (bootv_kseg ? BOOTV_KSEG1_BASE : BOOTV_KSEG0_BASE));
                    progress(1);
                }
            }
            printf(_("# done      \n"));
            if (! boot_dirty [devcfg_offset / blocksz]) {
                /* Write chip configuration. */
                if (FAMILY_MM == target->family->name_short){
                    target_program_devcfg(target, fdevopt, ficd, fpor, fwdt, 
                                            foscsel, fsec, afdevopt, aficd, 
                                            afpor, afwdt, afoscsel, afsec, 0, 0);
                }
				else if (FAMILY_MK == target->family->name_short){
					target_program_devcfg(target, bf1devcfg0, bf1devcfg1,
						bf1devcfg2, bf1devcfg3, bf1devcp, bf1devsign, bf1seq,
						bf2devcfg0, bf2devcfg1, bf2devcfg2, bf2devcfg3,
						bf2devcp, bf2devsign, bf2seq);
				}
                else{
                    target_program_devcfg(target, devcfg0, devcfg1, devcfg2, devcfg3,
                                            0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
                }
                boot_dirty [devcfg_offset / blocksz] = 1;
            }
        }
    }
    if (flash_used && !skip_verify) {
        printf(_(" Verify flash: "));
        print_symbols('.', progress_len);
        print_symbols('\b', progress_len);
        fflush(stdout);
        for (addr=0; addr<flash_bytes; addr+=blocksz) {
            if (flash_dirty [addr / blocksz]) {
                progress(progress_step);
                if (! verify_block(target, addr + (flashv_kseg ? FLASHV_KSEG1_BASE : FLASHV_KSEG0_BASE)))
                    exit(0);
            }
        }
        printf(_(" done\n"));
    }
    if (boot_used && !skip_verify) {
        printf(_("  Verify boot: "));
        print_symbols('.', boot_progress_len);
        print_symbols('\b', boot_progress_len);
        fflush(stdout);
        for (addr=0; addr<boot_bytes; addr+=blocksz) {
            if (boot_dirty [addr / blocksz]) {
                progress(1);
                if (! verify_block(target, addr + (bootv_kseg ? BOOTV_KSEG1_BASE : BOOTV_KSEG0_BASE)))
                    exit(0);
            }
        }
        printf(_(" done       \n"));
    }
    if (boot_used || flash_used)
        printf(_(" Program rate: %ld bytes per second\n"),
            total_bytes * 1000L / mseconds_elapsed(t0));
}

void do_read(char *filename, unsigned base, unsigned nbytes)
{
    FILE *fd;
    unsigned len, addr, data [256], progress_step;
    void *t0;

    fd = fopen(filename, "wb");
    if (! fd) {
        perror(filename);
        exit(1);
    }
    printf(_("       Memory: total %d bytes\n"), nbytes);

    /* Use 1kbyte blocks. */
    blocksz = 1024;

    /* Open and detect the device. */
    atexit(quit);
    target = target_open(target_port, target_speed, interface, interface_speed);
    if (! target) {
        fprintf(stderr, _("Error detecting device -- check cable!\n"));
        exit(1);
    }

    if ((target->adapter->flags & AD_READ) == 0) {
        fprintf(stderr, _("Error: Target read not supported.\n"));
        exit(1);
    }

    target_use_executive(target);
    for (progress_step=1; ; progress_step<<=1) {
        len = 1 + nbytes / progress_step / blocksz;
        if (len < 64)
            break;
    }
    printf("         Read: " );
    print_symbols('.', len);
    print_symbols('\b', len);
    fflush(stdout);

    progress_count = 0;
    t0 = fix_time();
    for (addr=base; addr-base<nbytes; addr+=blocksz) {
        progress(progress_step);
        target_read_block(target, addr, blocksz/4, data);
        if (fwrite(data, 1, blocksz, fd) != blocksz) {
            fprintf(stderr, "%s: write error!\n", filename);
            exit(1);
        }
    }
    printf(_("# done\n"));
    printf(_("         Rate: %ld bytes per second\n"),
        nbytes * 1000L / mseconds_elapsed(t0));
    fclose(fd);
}

/*
 * Print copying part of license
 */
static void gpl_show_copying(void)
{
    printf("%s.\n\n", copyright);
    printf("This program is free software; you can redistribute it and/or modify\n");
    printf("it under the terms of the GNU General Public License as published by\n");
    printf("the Free Software Foundation; either version 2 of the License, or\n");
    printf("(at your option) any later version.\n");
    printf("\n");
    printf("This program is distributed in the hope that it will be useful,\n");
    printf("but WITHOUT ANY WARRANTY; without even the implied warranty of\n");
    printf("MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n");
    printf("GNU General Public License for more details.\n");
    printf("\n");
}

/*
 * Print NO WARRANTY part of license
 */
static void gpl_show_warranty(void)
{
    printf("%s.\n\n", copyright);
    printf("BECAUSE THE PROGRAM IS LICENSED FREE OF CHARGE, THERE IS NO WARRANTY\n");
    printf("FOR THE PROGRAM, TO THE EXTENT PERMITTED BY APPLICABLE LAW.  EXCEPT WHEN\n");
    printf("OTHERWISE STATED IN WRITING THE COPYRIGHT HOLDERS AND/OR OTHER PARTIES\n");
    printf("PROVIDE THE PROGRAM \"AS IS\" WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED\n");
    printf("OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF\n");
    printf("MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.  THE ENTIRE RISK AS\n");
    printf("TO THE QUALITY AND PERFORMANCE OF THE PROGRAM IS WITH YOU.  SHOULD THE\n");
    printf("PROGRAM PROVE DEFECTIVE, YOU ASSUME THE COST OF ALL NECESSARY SERVICING,\n");
    printf("REPAIR OR CORRECTION.\n");
    printf("\n");
    printf("IN NO EVENT UNLESS REQUIRED BY APPLICABLE LAW OR AGREED TO IN WRITING\n");
    printf("WILL ANY COPYRIGHT HOLDER, OR ANY OTHER PARTY WHO MAY MODIFY AND/OR\n");
    printf("REDISTRIBUTE THE PROGRAM AS PERMITTED ABOVE, BE LIABLE TO YOU FOR DAMAGES,\n");
    printf("INCLUDING ANY GENERAL, SPECIAL, INCIDENTAL OR CONSEQUENTIAL DAMAGES ARISING\n");
    printf("OUT OF THE USE OR INABILITY TO USE THE PROGRAM (INCLUDING BUT NOT LIMITED\n");
    printf("TO LOSS OF DATA OR DATA BEING RENDERED INACCURATE OR LOSSES SUSTAINED BY\n");
    printf("YOU OR THIRD PARTIES OR A FAILURE OF THE PROGRAM TO OPERATE WITH ANY OTHER\n");
    printf("PROGRAMS), EVEN IF SUCH HOLDER OR OTHER PARTY HAS BEEN ADVISED OF THE\n");
    printf("POSSIBILITY OF SUCH DAMAGES.\n");
    printf("\n");
}

int main(int argc, char **argv)
{
    int ch, read_mode = 0;
    unsigned base, nbytes;
    static const struct option long_options[] = {
        { "help",        0, 0, 'h' },
        { "warranty",    0, 0, 'W' },
        { "copying",     0, 0, 'C' },
        { "version",     0, 0, 'V' },
        { "skip-verify", 0, 0, 'S' },
        { NULL,          0, 0, 0 },
    };

    /* Set locale and message catalogs. */
    setlocale(LC_ALL, "");
#if defined(__CYGWIN32__) || defined(MINGW32)
    /* Files with localized messages should be placed in
     * the current directory or in c:/Program Files/pic32prog. */
    if (access("./ru/LC_MESSAGES/pic32prog.mo", R_OK) == 0)
        bindtextdomain("pic32prog", ".");
    else
        bindtextdomain("pic32prog", "c:/Program Files/pic32prog");
#else
    bindtextdomain("pic32prog", "/usr/local/share/locale");
#endif
    textdomain("pic32prog");

    setvbuf(stdout, (char *)NULL, _IOLBF, 0);
    setvbuf(stderr, (char *)NULL, _IOLBF, 0);
    printf(_("Programmer for Microchip PIC32 microcontrollers, Version %s\n"), VERSION);
    progname = argv[0];
    copyright = _("    Copyright: (C) 2011-2015 Serge Vakulenko");
    signal(SIGINT, interrupted);
#ifdef __linux__
    signal(SIGHUP, interrupted);
#endif
    signal(SIGTERM, interrupted);

    while ((ch = getopt_long(argc, argv, "vDhrpeCVWSd:b:B:i:s:",
      long_options, 0)) != -1) {
        switch (ch) {
        case 'v':
            ++verify_only;
            continue;
        case 'D':
            ++debug_level;
            continue;
        case 'r':
            ++read_mode;
            continue;
        case 'p':
            ++power_on;
            continue;
        case 'e':
            ++erase_only;
            continue;
        case 'd':
            target_port = optarg;
            continue;
        case 'b':
            target_speed = strtoul(optarg, 0, 0);
            if (strncasecmp("ascii:", target_port, 6) != 0)                 // *** HORRIBLE HACK!! ***
            if (! serial_speed_valid(target_speed))
                return 0;
            // If the alternate hasn't changed from default then keep
            // it the same as the master speed
            if (alternate_speed == 115200) {
                alternate_speed = target_speed;
            }
            continue;
        case 'B':
            alternate_speed = strtoul(optarg, 0, 0);
            if (! serial_speed_valid(alternate_speed)) {
                printf("Debug: %d\n", alternate_speed);
                return 0;
            }
            continue;
        case 'h':
            break;
        case 'V':
            /* Version already printed above. */
            return 0;
        case 'C':
            gpl_show_copying();
            return 0;
        case 'W':
            gpl_show_warranty();
            return 0;
        case 'S':
            ++skip_verify;
            continue;
        case 'i':
            if (strcmp(optarg, "jtag") == 0 || strcmp(optarg, "JTAG") == 0){
                interface = INTERFACE_JTAG;
                if (debug_level > 0){
                    fprintf(stderr, "Using JTAG interface, if available\n");
                }
            }
            else if ( strcmp(optarg, "icsp") == 0 || strcmp(optarg, "ICSP") == 0){
                interface = INTERFACE_ICSP;
                if (debug_level > 0){
                    fprintf(stderr, "Using ICSP interface, if available\n");
                }
            }
            else{
                fprintf(stderr, "Unknown interface \"%s\" specified\n", optarg);
                return 0;
            }
            continue;
        case 's':
            interface_speed = strtoul(optarg, 0, 0);
            if (debug_level > 0){
                fprintf(stderr, "Using clock speed of %d khz, if available\n", interface_speed);
            }
            continue;
        }
usage:
        printf("%s.\n\n", copyright);
        printf("PIC32prog comes with ABSOLUTELY NO WARRANTY; for details\n");
        printf("use `--warranty' option. This is Open Source software. You are\n");
        printf("welcome to redistribute it under certain conditions. Use the\n");
        printf("'--copying' option for details.\n\n");
        printf("Probe:\n");
        printf("       pic32prog\n");
        printf("\nWrite flash memory:\n");
        printf("       pic32prog [-v] file.srec\n");
        printf("       pic32prog [-v] file.hex\n");
        printf("\nRead memory:\n");
        printf("       pic32prog -r file.bin address length\n");
        printf("\nArgs:\n");
        printf("       file.srec           Code file in SREC format\n");
        printf("       file.hex            Code file in Intel HEX format\n");
        printf("       file.bin            Code file in binary format\n");
        printf("       -v                  Verify only\n");
        printf("       -r                  Read mode\n");
        printf("       -d device           Use specified serial or USB device\n");
        printf("       -b baudrate         Serial speed, default 115200\n");
        printf("       -B alt_baud         Request an alternative baud rate\n");
        printf("       -i interface        Choose JTAG or ICSP (if supported)\n");
        printf("       -s clock_speed      Speed of interface in khz, if supported\n");
        printf("       -e                  Erase chip\n");
        printf("       -p                  Leave board powered on\n");
        printf("       -D                  Debug mode\n");
        printf("       -h, --help          Print this help message\n");
        printf("       -V, --version       Print version\n");
        printf("       -C, --copying       Print copying information\n");
        printf("       -W, --warranty      Print warranty information\n");
        printf("       -S, --skip-verify   Skip the write verification step\n");
        printf("\n");
        return 0;
    }
    printf("%s\n", copyright);
    argc -= optind;
    argv += optind;

    memset(boot_data, ~0, BOOT_BYTES);
    memset(flash_data, ~0, FLASH_BYTES);

    switch (argc) {
    case 0:
        if (erase_only > 0) {
            do_erase();
        } else {
            do_probe();
        }
        break;
    case 1:
        if (! read_srec(argv[0]) &&
            ! read_hex(argv[0])) {
            fprintf(stderr, _("%s: bad file format\n"), argv[0]);
            exit(1);
        }
        do_program(argv[0]);
        break;
    case 3:
        if (! read_mode)
            goto usage;
        base = strtoul(argv[1], 0, 0);
        nbytes = strtoul(argv[2], 0, 0);
        do_read(argv[0], base, nbytes);
        break;
    default:
        goto usage;
    }
    quit();
    return 0;
}
