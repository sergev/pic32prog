/*
 * Flash memory programmer for Microchip PIC32 microcontrollers.
 *
 * Copyright (C) 2011 Serge Vakulenko
 *
 * This file is part of BKUNIX project, which is distributed
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
#include "localize.h"

#define VERSION         "1.0"
#define BLOCKSZ         1024
#define FLASHV_BASE     0x9d000000
#define BOOTV_BASE      0x9fc00000
#define CONFIGV_BASE    0x9fc02ff0
#define FLASHP_BASE     0x1d000000
#define BOOTP_BASE      0x1fc00000
#define CONFIGP_BASE    0x1fc02ff0
#define FLASH_KBYTES    512
#define BOOT_KBYTES     12
#define FLASH_SIZE      (FLASH_KBYTES * 1024)
#define BOOT_SIZE       (BOOT_KBYTES * 1024)

/* Macros for converting between hex and binary. */
#define NIBBLE(x)       (isdigit(x) ? (x)-'0' : tolower(x)+10-'a')
#define HEX(buffer)     ((NIBBLE((buffer)[0])<<4) + NIBBLE((buffer)[1]))

/* Data to write */
unsigned char boot_data [BOOT_SIZE];
unsigned char flash_data [FLASH_SIZE];
unsigned char boot_dirty [BOOT_KBYTES];
unsigned char flash_dirty [FLASH_KBYTES];
unsigned flash_used;
int total_bytes;

#define DEVCFG3 *(unsigned*) (boot_data + BOOT_SIZE - 16)
#define DEVCFG2 *(unsigned*) (boot_data + BOOT_SIZE - 12)
#define DEVCFG1 *(unsigned*) (boot_data + BOOT_SIZE - 8)
#define DEVCFG0 *(unsigned*) (boot_data + BOOT_SIZE - 4)

unsigned progress_count;
int verify_only;
int debug_level;
int power_on;
target_t *target;
char *progname;
const char *copyright;

void *fix_time ()
{
    static struct timeval t0;

    gettimeofday (&t0, 0);
    return &t0;
}

unsigned mseconds_elapsed (void *arg)
{
    struct timeval t1, *t0 = arg;
    unsigned mseconds;

    gettimeofday (&t1, 0);
    mseconds = (t1.tv_sec - t0->tv_sec) * 1000 +
        (t1.tv_usec - t0->tv_usec) / 1000;
    if (mseconds < 1)
        mseconds = 1;
    return mseconds;
}

void store_data (unsigned address, unsigned byte)
{
    unsigned offset;

    if (address >= BOOTV_BASE && address < BOOTV_BASE + BOOT_SIZE) {
        /* Boot code, virtual. */
        offset = address - BOOTV_BASE;
        boot_data [offset] = byte;
        if (address < CONFIGV_BASE)
            boot_dirty [offset / 1024] = 1;

    } else if (address >= BOOTP_BASE && address < BOOTP_BASE + BOOT_SIZE) {
        /* Boot code, physical. */
        offset = address - BOOTP_BASE;
        boot_data [offset] = byte;
        if (address < CONFIGP_BASE)
            boot_dirty [offset / 1024] = 1;

    } else if (address >= FLASHV_BASE && address < FLASHV_BASE + FLASH_SIZE) {
        /* Main flash memory, virtual. */
        offset = address - FLASHV_BASE;
        flash_data [offset] = byte;
        flash_dirty [offset / 1024] = 1;
        flash_used = 1;

    } else if (address >= FLASHP_BASE && address < FLASHP_BASE + FLASH_SIZE) {
        /* Main flash memory, physical. */
        offset = address - FLASHP_BASE;
        flash_data [offset] = byte;
        flash_dirty [offset / 1024] = 1;
        flash_used = 1;
    } else {
        fprintf (stderr, _("%08X: address out of flash memory\n"), address);
        exit (1);
    }
    total_bytes++;
}

/*
 * Read the S record file.
 */
int read_srec (char *filename)
{
    FILE *fd;
    unsigned char buf [256];
    unsigned char *data;
    unsigned address;
    int bytes;

    fd = fopen (filename, "r");
    if (! fd) {
        perror (filename);
        exit (1);
    }
    while (fgets ((char*) buf, sizeof(buf), fd)) {
        if (buf[0] == '\n')
            continue;
        if (buf[0] != 'S') {
            fclose (fd);
            return 0;
        }
        if (buf[1] == '7' || buf[1] == '8' || buf[1] == '9')
            break;

        /* Starting an S-record.  */
        if (! isxdigit (buf[2]) || ! isxdigit (buf[3])) {
            fprintf (stderr, _("%s: bad SREC record: %s\n"), filename, buf);
            exit (1);
        }
        bytes = HEX (buf + 2);

        /* Ignore the checksum byte.  */
        --bytes;

        address = 0;
        data = buf + 4;
        switch (buf[1]) {
        case '3':
            address = HEX (data);
            data += 2;
            --bytes;
            /* Fall through.  */
        case '2':
            address = (address << 8) | HEX (data);
            data += 2;
            --bytes;
            /* Fall through.  */
        case '1':
            address = (address << 8) | HEX (data);
            data += 2;
            address = (address << 8) | HEX (data);
            data += 2;
            bytes -= 2;

            while (bytes-- > 0) {
                store_data (address++, HEX (data));
                data += 2;
            }
            break;
        }
    }
    fclose (fd);
    return 1;
}

/*
 * Read HEX file.
 */
int read_hex (char *filename)
{
    FILE *fd;
    unsigned char buf [256], data[16], record_type, sum;
    unsigned address, high;
    int bytes, i;

    fd = fopen (filename, "r");
    if (! fd) {
        perror (filename);
        exit (1);
    }
    high = 0;
    while (fgets ((char*) buf, sizeof(buf), fd)) {
        if (buf[0] == '\n')
            continue;
        if (buf[0] != ':') {
            fclose (fd);
            return 0;
        }
        if (! isxdigit (buf[1]) || ! isxdigit (buf[2]) ||
            ! isxdigit (buf[3]) || ! isxdigit (buf[4]) ||
            ! isxdigit (buf[5]) || ! isxdigit (buf[6]) ||
            ! isxdigit (buf[7]) || ! isxdigit (buf[8])) {
            fprintf (stderr, _("%s: bad HEX record: %s\n"), filename, buf);
            exit (1);
        }
	record_type = HEX (buf+7);
	if (record_type == 1) {
	    /* End of file. */
            break;
        }
	if (record_type == 5) {
	    /* Start address, ignore. */
	    continue;
	}

	bytes = HEX (buf+1);
        if (bytes & 1) {
            fprintf (stderr, _("%s: odd length\n"), filename);
            exit (1);
        }
	if (strlen ((char*) buf) < bytes * 2 + 11) {
            fprintf (stderr, _("%s: too short hex line\n"), filename);
            exit (1);
        }
	address = high << 16 | HEX (buf+3) << 8 | HEX (buf+5);
        if (address & 3) {
            fprintf (stderr, _("%s: odd address\n"), filename);
            exit (1);
        }

	sum = 0;
	for (i=0; i<bytes; ++i) {
            data [i] = HEX (buf+9 + i + i);
	    sum += data [i];
	}
	sum += record_type + bytes + (address & 0xff) + (address >> 8 & 0xff);
	if (sum != (unsigned char) - HEX (buf+9 + bytes + bytes)) {
            fprintf (stderr, _("%s: bad HEX checksum\n"), filename);
            exit (1);
        }

	if (record_type == 4) {
	    /* Extended address. */
            if (bytes != 2) {
                fprintf (stderr, _("%s: invalid HEX linear address record length\n"),
                    filename);
                exit (1);
            }
	    high = data[0] << 8 | data[1];
	    continue;
	}
	if (record_type != 0) {
            fprintf (stderr, _("%s: unknown HEX record type: %d\n"),
                filename, record_type);
            exit (1);
        }

        for (i=0; i<bytes; i++) {
            store_data (address++, data [i]);
        }
    }
    fclose (fd);
    return 1;
}

void print_symbols (char symbol, int cnt)
{
    while (cnt-- > 0)
        putchar (symbol);
}

void progress (unsigned step)
{
    ++progress_count;
    if (progress_count % step == 0) {
        putchar ('#');
        fflush (stdout);
    }
}

void quit (void)
{
    if (target != 0) {
        target_close (target, power_on);
        free (target);
        target = 0;
    }
}

void interrupted (int signum)
{
    fprintf (stderr, _("\nInterrupted.\n"));
    quit();
    _exit (-1);
}

void do_probe ()
{
    /* Open and detect the device. */
    atexit (quit);
    target = target_open ();
    if (! target) {
        fprintf (stderr, _("Error detecting device -- check cable!\n"));
        exit (1);
    }
    printf (_("    Processor: %s (id %08X)\n"), target_cpu_name (target),
        target_idcode (target));
    printf (_(" Flash memory: %d kbytes\n"), target_flash_bytes (target) / 1024);
    target_print_devcfg (target);
}

/*
 * Write flash memory.
 */
void program_block (target_t *mc, unsigned addr)
{
    unsigned char *data;
    unsigned offset;

    if (addr >= BOOTV_BASE && addr < BOOTV_BASE+BOOT_SIZE) {
        data = boot_data;
        offset = addr - BOOTV_BASE;
    } else if (addr >= BOOTP_BASE && addr < BOOTP_BASE+BOOT_SIZE) {
        data = boot_data;
        offset = addr - BOOTP_BASE;
    } else if (addr >= FLASHV_BASE && addr < FLASHV_BASE + FLASH_SIZE) {
        data = flash_data;
        offset = addr - FLASHV_BASE;
    } else {
        data = flash_data;
        offset = addr - FLASHP_BASE;
    }
    target_program_block (mc, addr, BLOCKSZ/4, (unsigned*) (data + offset));
}

int verify_block (target_t *mc, unsigned addr)
{
    int i;
    unsigned word, expected, block [BLOCKSZ/4];
    unsigned char *data;
    unsigned offset;

    if (addr >= BOOTV_BASE && addr < BOOTV_BASE+BOOT_SIZE) {
        data = boot_data;
        offset = addr - BOOTV_BASE;
    } else if (addr >= BOOTP_BASE && addr < BOOTP_BASE+BOOT_SIZE) {
        data = boot_data;
        offset = addr - BOOTP_BASE;
    } else if (addr >= FLASHV_BASE && addr < FLASHV_BASE + FLASH_SIZE) {
        data = flash_data;
        offset = addr - FLASHV_BASE;
    } else {
        data = flash_data;
        offset = addr - FLASHP_BASE;
    }
    target_read_block (mc, addr, BLOCKSZ/4, block);
    for (i=0; i<BLOCKSZ; i+=4) {
        expected = *(unsigned*) (data + offset + i);
        word = block [i/4];
        if (word != expected) {
            printf (_("\nerror at address %08X: file=%08X, mem=%08X\n"),
                addr + i, expected, word);
            exit (1);
        }
    }
    return 1;
}

void do_program (char *filename)
{
    unsigned addr;
    int progress_len, progress_step;
    void *t0;

    /* Open and detect the device. */
    atexit (quit);
    target = target_open ();
    if (! target) {
        fprintf (stderr, _("Error detecting device -- check cable!\n"));
        exit (1);
    }
    printf (_("    Processor: %s\n"), target_cpu_name (target));
    printf (_(" Flash memory: %d kbytes\n"), target_flash_bytes (target) / 1024);
    printf (_("         Data: %d bytes\n"), total_bytes);
    if (DEVCFG0 & 0x80000000) {
        /* Default configuration. */
        DEVCFG0 = 0x7ffffffd;
        DEVCFG1 = 0xff6afd5b;
        DEVCFG2 = 0xfff879d9;
        DEVCFG3 = 0x3affffff;
    }

    if (! verify_only) {
        /* Erase flash. */
        target_erase (target);
    }
    target_use_executable (target);
    for (progress_step=1; ; progress_step<<=1) {
        progress_len = 1 + total_bytes / progress_step / BLOCKSZ;
        if (progress_len < 64)
            break;
    }

    progress_count = 0;
    t0 = fix_time ();
    if (! verify_only) {
        if (flash_used) {
            printf (_("Program flash: "));
            print_symbols ('.', progress_len);
            print_symbols ('\b', progress_len);
            fflush (stdout);
            for (addr=FLASHV_BASE; addr-FLASHV_BASE<FLASH_SIZE; addr+=BLOCKSZ) {
                if (flash_dirty [(addr-FLASHV_BASE) / 1024]) {
                    program_block (target, addr);
                    progress (progress_step);
                }
            }
            printf (_("# done\n"));
        }
	printf (_(" Program boot: ............\b\b\b\b\b\b\b\b\b\b\b\b"));
        fflush (stdout);
        for (addr=BOOTV_BASE; addr-BOOTV_BASE<BOOT_SIZE; addr+=BLOCKSZ) {
            if (boot_dirty [(addr-BOOTV_BASE) / 1024]) {
                program_block (target, addr);
                progress (1);
            }
        }
        printf (_("# done\n"));
        if (! boot_dirty [BOOT_KBYTES-1]) {
            /* Write chip configuration. */
            target_program_word (target, BOOTV_BASE + BOOT_SIZE - 16, DEVCFG3);
            target_program_word (target, BOOTV_BASE + BOOT_SIZE - 12, DEVCFG2);
            target_program_word (target, BOOTV_BASE + BOOT_SIZE - 8, DEVCFG1);
            target_program_word (target, BOOTV_BASE + BOOT_SIZE - 4, DEVCFG0);
            boot_dirty [BOOT_KBYTES-1] = 1;
        }
    }
    if (flash_used) {
        printf (_(" Verify flash: "));
        print_symbols ('.', progress_len);
        print_symbols ('\b', progress_len);
        fflush (stdout);
        for (addr=FLASHV_BASE; addr-FLASHV_BASE<FLASH_SIZE; addr+=BLOCKSZ) {
            if (flash_dirty [(addr-FLASHV_BASE) / 1024]) {
                progress (progress_step);
                if (! verify_block (target, addr))
                    exit (0);
            }
        }
        printf (_("# done\n"));
    }
    printf (_("  Verify boot: ............\b\b\b\b\b\b\b\b\b\b\b\b"));
    fflush (stdout);
    for (addr=BOOTV_BASE; addr-BOOTV_BASE<BOOT_SIZE; addr+=BLOCKSZ) {
        if (boot_dirty [(addr-BOOTV_BASE) / 1024]) {
            progress (1);
            if (! verify_block (target, addr))
                exit (0);
        }
    }
    printf (_("# done\n"));
    printf (_("Rate: %ld bytes per second\n"),
        total_bytes * 1000L / mseconds_elapsed (t0));
}

void do_read (char *filename, unsigned base, unsigned nbytes)
{
    FILE *fd;
    unsigned len, addr, data [BLOCKSZ/4], progress_step;
    void *t0;

    fd = fopen (filename, "wb");
    if (! fd) {
        perror (filename);
        exit (1);
    }
    printf (_("Memory: total %d bytes\n"), nbytes);

    /* Open and detect the device. */
    atexit (quit);
    target = target_open ();
    if (! target) {
        fprintf (stderr, _("Error detecting device -- check cable!\n"));
        exit (1);
    }
    target_use_executable (target);
    for (progress_step=1; ; progress_step<<=1) {
        len = 1 + nbytes / progress_step / BLOCKSZ;
        if (len < 64)
            break;
    }
    printf ("Read: " );
    print_symbols ('.', len);
    print_symbols ('\b', len);
    fflush (stdout);

    progress_count = 0;
    t0 = fix_time ();
    for (addr=base; addr-base<nbytes; addr+=BLOCKSZ) {
        progress (progress_step);
        target_read_block (target, addr, BLOCKSZ/4, data);
        if (fwrite (data, 1, BLOCKSZ, fd) != BLOCKSZ) {
            fprintf (stderr, "%s: write error!\n", filename);
            exit (1);
        }
    }
    printf (_("# done\n"));
    printf (_("Rate: %ld bytes per second\n"),
        nbytes * 1000L / mseconds_elapsed (t0));
    fclose (fd);
}

/*
 * Print copying part of license
 */
static void gpl_show_copying (void)
{
    printf ("%s.\n\n", copyright);
    printf ("This program is free software; you can redistribute it and/or modify\n");
    printf ("it under the terms of the GNU General Public License as published by\n");
    printf ("the Free Software Foundation; either version 2 of the License, or\n");
    printf ("(at your option) any later version.\n");
    printf ("\n");
    printf ("This program is distributed in the hope that it will be useful,\n");
    printf ("but WITHOUT ANY WARRANTY; without even the implied warranty of\n");
    printf ("MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n");
    printf ("GNU General Public License for more details.\n");
    printf ("\n");
}

/*
 * Print NO WARRANTY part of license
 */
static void gpl_show_warranty (void)
{
    printf ("%s.\n\n", copyright);
    printf ("BECAUSE THE PROGRAM IS LICENSED FREE OF CHARGE, THERE IS NO WARRANTY\n");
    printf ("FOR THE PROGRAM, TO THE EXTENT PERMITTED BY APPLICABLE LAW.  EXCEPT WHEN\n");
    printf ("OTHERWISE STATED IN WRITING THE COPYRIGHT HOLDERS AND/OR OTHER PARTIES\n");
    printf ("PROVIDE THE PROGRAM \"AS IS\" WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED\n");
    printf ("OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF\n");
    printf ("MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.  THE ENTIRE RISK AS\n");
    printf ("TO THE QUALITY AND PERFORMANCE OF THE PROGRAM IS WITH YOU.  SHOULD THE\n");
    printf ("PROGRAM PROVE DEFECTIVE, YOU ASSUME THE COST OF ALL NECESSARY SERVICING,\n");
    printf ("REPAIR OR CORRECTION.\n");
    printf("\n");
    printf ("IN NO EVENT UNLESS REQUIRED BY APPLICABLE LAW OR AGREED TO IN WRITING\n");
    printf ("WILL ANY COPYRIGHT HOLDER, OR ANY OTHER PARTY WHO MAY MODIFY AND/OR\n");
    printf ("REDISTRIBUTE THE PROGRAM AS PERMITTED ABOVE, BE LIABLE TO YOU FOR DAMAGES,\n");
    printf ("INCLUDING ANY GENERAL, SPECIAL, INCIDENTAL OR CONSEQUENTIAL DAMAGES ARISING\n");
    printf ("OUT OF THE USE OR INABILITY TO USE THE PROGRAM (INCLUDING BUT NOT LIMITED\n");
    printf ("TO LOSS OF DATA OR DATA BEING RENDERED INACCURATE OR LOSSES SUSTAINED BY\n");
    printf ("YOU OR THIRD PARTIES OR A FAILURE OF THE PROGRAM TO OPERATE WITH ANY OTHER\n");
    printf ("PROGRAMS), EVEN IF SUCH HOLDER OR OTHER PARTY HAS BEEN ADVISED OF THE\n");
    printf ("POSSIBILITY OF SUCH DAMAGES.\n");
    printf("\n");
}

int main (int argc, char **argv)
{
    int ch, read_mode = 0;
    unsigned base, nbytes;
    static const struct option long_options[] = {
        { "help",        0, 0, 'h' },
        { "warranty",    0, 0, 'W' },
        { "copying",     0, 0, 'C' },
        { "version",     0, 0, 'V' },
        { NULL,          0, 0, 0 },
    };

    /* Set locale and message catalogs. */
    setlocale (LC_ALL, "");
#if defined (__CYGWIN32__) || defined (MINGW32)
    /* Files with localized messages should be placed in
     * the current directory or in c:/Program Files/pic32prog. */
    if (access ("./ru/LC_MESSAGES/pic32prog.mo", R_OK) == 0)
        bindtextdomain ("pic32prog", ".");
    else
        bindtextdomain ("pic32prog", "c:/Program Files/pic32prog");
#else
    bindtextdomain ("pic32prog", "/usr/local/share/locale");
#endif
    textdomain ("pic32prog");

    setvbuf (stdout, (char *)NULL, _IOLBF, 0);
    setvbuf (stderr, (char *)NULL, _IOLBF, 0);
    printf (_("Programmer for Mictochip PIC32 microcontrollers, Version %s\n"), VERSION);
    progname = argv[0];
    copyright = _("    Copyright: (C) 2011 Serge Vakulenko");
    signal (SIGINT, interrupted);
#ifdef __linux__
    signal (SIGHUP, interrupted);
#endif
    signal (SIGTERM, interrupted);

    while ((ch = getopt_long (argc, argv, "vDhrpCVW",
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
        case 'h':
            break;
        case 'V':
            /* Version already printed above. */
            return 0;
        case 'C':
            gpl_show_copying ();
            return 0;
        case 'W':
            gpl_show_warranty ();
            return 0;
        }
usage:
        printf ("%s.\n\n", copyright);
        printf ("PIC32prog comes with ABSOLUTELY NO WARRANTY; for details\n");
        printf ("use `--warranty' option. This is Open Source software. You are\n");
        printf ("welcome to redistribute it under certain conditions. Use the\n");
        printf ("'--copying' option for details.\n\n");
        printf ("Probe:\n");
        printf ("       pic32prog\n");
        printf ("\nWrite flash memory:\n");
        printf ("       pic32prog [-v] file.srec\n");
        printf ("       pic32prog [-v] file.hex\n");
        printf ("\nRead memory:\n");
        printf ("       pic32prog -r file.bin address length\n");
        printf ("\nArgs:\n");
        printf ("       file.srec           Code file in SREC format\n");
        printf ("       file.hex            Code file in Intel HEX format\n");
        printf ("       file.bin            Code file in binary format\n");
        printf ("       -v                  Verify only\n");
        printf ("       -r                  Read mode\n");
        printf ("       -p                  Leave board powered on\n");
        printf ("       -D                  Debug mode\n");
        printf ("       -h, --help          Print this help message\n");
        printf ("       -V, --version       Print version\n");
        printf ("       -C, --copying       Print copying information\n");
        printf ("       -W, --warranty      Print warranty information\n");
        printf ("\n");
        return 0;
    }
    printf ("%s\n", copyright);
    argc -= optind;
    argv += optind;

    memset (boot_data, ~0, BOOT_SIZE);
    memset (flash_data, ~0, FLASH_SIZE);

    switch (argc) {
    case 0:
        do_probe ();
        break;
    case 1:
        if (! read_srec (argv[0]) &&
            ! read_hex (argv[0])) {
            fprintf (stderr, _("%s: bad file format\n"), argv[0]);
            exit (1);
        }
        do_program (argv[0]);
        break;
    case 3:
        if (! read_mode)
            goto usage;
        base = strtoul (argv[1], 0, 0);
        nbytes = strtoul (argv[2], 0, 0);
        do_read (argv[0], base, nbytes);
        break;
    default:
        goto usage;
    }
    quit ();
    return 0;
}
