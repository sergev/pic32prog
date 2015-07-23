/*
 * Interface to mikroE USB HID Bootloader (UHB).
 *
 * Copyright (C) 2014 Serge Vakulenko
 *
 * This file is part of PIC32PROG project, which is distributed
 * under the terms of the GNU General Public License (GPL).
 * See the accompanying file "COPYING" for more details.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#include "adapter.h"
#include "hidapi.h"
#include "pic32.h"

/* Bootloader commands */
#define CMD_NON     0           /* 'Idle' */
#define CMD_SYNC    1           /* Synchronize with PC tool */
#define CMD_INFO    2           /* Send bootloader info record */
#define CMD_BOOT    3           /* Go to bootloader mode */
#define CMD_REBOOT  4           /* Restart MCU */
#define CMD_WRITE   11          /* Write to MCU flash */
#define CMD_ERASE   21          /* Erase MCU flash */

#define STX         15          /* Start of TeXt */

typedef struct {
    /* Common part */
    adapter_t adapter;

    /* Device handle for libusb. */
    hid_device *hiddev;

    unsigned flash_size;
    unsigned erase_size;
    unsigned write_size;
    unsigned version;
    unsigned boot_start;
    unsigned boot_erased;
    char name [32];

    unsigned char reply [64];

} uhb_adapter_t;

/*
 * Identifiers of USB adapter.
 */
#define MIKROE_VID              0x1234
#define MIKROEBOOT_PID          0x0001  /* MikroElektronika HID bootloader */

/*
 * Send a request to the device.
 * Store the reply into the a->reply[] array.
 */
static void uhb_command (uhb_adapter_t *a, unsigned char cmd,
    unsigned addr, unsigned count, unsigned char *data, unsigned data_bytes)
{
    unsigned char buf [64];
    unsigned k, nbytes = 2;
    int reply_len;

    /* Send command packet. */
    memset (buf, 0, sizeof(buf));
    buf[0] = STX;
    buf[1] = cmd;
    if (cmd == CMD_WRITE || cmd == CMD_ERASE) {
        /*
         * Write and Erase command format:
         *
         *  <STX> <CMD> <Address[0..3]> <Count[0..1]>
         * |- 1 -|- 1 -|------ 4 ------|----- 2 -----|
         */
        buf[2] = addr;
        buf[3] = addr >> 8;
        buf[4] = addr >> 16;
        buf[5] = addr >> 24;
        buf[6] = count;
        buf[7] = count >> 8;
        nbytes += 6;
    }

    if (debug_level > 0) {
        fprintf (stderr, "---Send");
        for (k=0; k<nbytes; ++k) {
            if (k != 0 && (k & 15) == 0)
                fprintf (stderr, "\n       ");
            fprintf (stderr, " %02x", buf[k]);
        }
        fprintf (stderr, "\n");
    }
    hid_write (a->hiddev, buf, 64);

    if (cmd == CMD_REBOOT) {
        /* No reply expected. */
        return;
    }

    if (cmd == CMD_WRITE) {
        /* Send data. */
        for (; data_bytes>0; data_bytes-=64) {
            if (debug_level > 0) {
                fprintf (stderr, "---    ");
                for (k=0; k<64; ++k) {
                    if (k != 0 && (k & 15) == 0)
                        fprintf (stderr, "\n       ");
                    fprintf (stderr, " %02x", data[k]);
                }
                fprintf (stderr, "\n");
            }
            hid_write (a->hiddev, data, 64);
            data += 64;
        }
    }

    /* Get reply. */
    memset (a->reply, 0, sizeof(a->reply));
    reply_len = hid_read_timeout (a->hiddev, a->reply, 64, 500);
    if (reply_len == 0) {
        fprintf (stderr, "Timed out.\n");
        exit (-1);
    }
    if (reply_len != 64) {
        fprintf (stderr, "uhb: error %d receiving packet\n", reply_len);
        exit (-1);
    }
    if (debug_level > 0) {
        fprintf (stderr, "---Recv");
        for (k=0; k<2; ++k) {
            if (k != 0 && (k & 15) == 0)
                fprintf (stderr, "\n       ");
            fprintf (stderr, " %02x", a->reply[k]);
        }
        fprintf (stderr, "\n");
    }
}

static void uhb_close (adapter_t *adapter, int power_on)
{
    uhb_adapter_t *a = (uhb_adapter_t*) adapter;

    /* Jump to application. */
    uhb_command (a, CMD_REBOOT, 0, 0, 0, 0);
    free (a);
}

/*
 * Return the Device Identification code
 */
static unsigned uhb_get_idcode (adapter_t *adapter)
{
    return 0xDEAFB00B;
}

/*
 * Read a configuration word from memory.
 */
static unsigned uhb_read_word (adapter_t *adapter, unsigned addr)
{
    /* Not supported by UHB bootloader. */
    return 0;
}

/*
 * Write a configuration word to flash memory.
 */
static void uhb_program_word (adapter_t *adapter,
    unsigned addr, unsigned word)
{
    /* Not supported by UHB bootloader. */
    if (debug_level > 0)
        fprintf (stderr, "uhb: program word at %08x: %08x\n", addr, word);
}

/*
 * Verify a block of memory (1024 bytes).
 */
static void uhb_verify_data (adapter_t *adapter,
    unsigned addr, unsigned nwords, unsigned *data)
{
    /* Not supported by UHB bootloader. */
}

/*
 * Flash write, 1-kbyte blocks.
 */
static void uhb_program_block (adapter_t *adapter,
    unsigned addr, unsigned *data)
{
    uhb_adapter_t *a = (uhb_adapter_t*) adapter;

    if (debug_level > 0)
        fprintf (stderr, "uhb: program 1024 bytes at %08x: %08x-%08x-...-%08x\n",
            addr, data[0], data[1], data[255]);

    if (! (addr >= a->adapter.user_start &&
           addr + 1024 <= a->adapter.user_start + a->adapter.user_nbytes) &&
        ! (addr >= 0x1fc00000 &&
           addr + 1024 <= 0x1fc00000 + 8*1024)) {
        fprintf (stderr, "address %08x out of program area\n", addr);
        return;
    }

    if (addr >= 0x1fc00000 &&
        addr + 1024 <= 0x1fc00000 + 8*1024 &&
        ! a->boot_erased)
    {
        /* Erase boot area. */
        unsigned nblocks = 8*1024 / a->erase_size;
        unsigned ba = 0x1fc00000;
        for (; nblocks-- > 0; addr += a->erase_size) {
            if (debug_level > 0)
                fprintf (stderr, "*** uhb: erase boot block %08x\n", ba);

            /* Erase one block. */
            uhb_command (a, CMD_ERASE, ba, 1, 0, 0);
        }
        a->boot_erased = 1;
    }

    uhb_command (a, CMD_WRITE, addr, 1024, (unsigned char*)data, 1024);
}

/*
 * Erase all flash memory.
 */
static void uhb_erase_chip (adapter_t *adapter)
{
    uhb_adapter_t *a = (uhb_adapter_t*) adapter;
    int nblocks;
    unsigned addr;

    /* Erase user flash memory. */
    nblocks = a->adapter.user_nbytes / a->erase_size;
    for (addr = a->adapter.user_start; nblocks-- > 0; addr += a->erase_size) {
        if (debug_level > 0)
            fprintf (stderr, "*** uhb: erase flash block %08x\n", addr);

        /* Erase one block. */
        uhb_command (a, CMD_ERASE, addr, 1, 0, 0);
    }
}

/*
 * Initialize adapter uhb.
 * Return a pointer to a data structure, allocated dynamically.
 * When adapter not found, return 0.
 */
adapter_t *adapter_open_uhb (void)
{
    uhb_adapter_t *a;
    hid_device *hiddev;

    hiddev = hid_open (MIKROE_VID, MIKROEBOOT_PID, 0);
    if (! hiddev) {
        /*fprintf (stderr, "HID bootloader not found\n");*/
        return 0;
    }
    a = calloc (1, sizeof (*a));
    if (! a) {
        fprintf (stderr, "Out of memory\n");
        return 0;
    }
    a->hiddev = hiddev;

    /* Read version of adapter. */
    uhb_command (a, CMD_INFO, 0, 0, 0, 0);
    if (a->reply[0] != 56 ||                /* Info packet size */
        a->reply[1] != 1 ||                 /* Tag: MCU type */
        a->reply[2] != 20 ||                /* PIC32 family */
        a->reply[4] != 8 ||                 /* Tag: flash size */
        a->reply[12] != 3 ||                /* Tag: erase block size */
        a->reply[16] != 4 ||                /* Tag: write block size */
        a->reply[20] != 5 ||                /* Tag: version of bootloader */
        a->reply[24] != 6 ||                /* Tag: bootloader start address */
        a->reply[32] != 7)                  /* Tag: board name */
        return 0;

    a->flash_size  = a->reply[8] | (a->reply[9] << 8) |
                     (a->reply[10] << 16) | (a->reply[11] << 24);
    a->erase_size  = a->reply[14] | (a->reply[15] << 8);
    a->write_size  = a->reply[18] | (a->reply[19] << 8);
    a->version     = a->reply[22] | (a->reply[23] << 8);
    a->boot_start  = a->reply[28] | (a->reply[29] << 8) |
                     (a->reply[30] << 16) | (a->reply[31] << 24);
    memcpy (a->name, &a->reply[33], 31);

    a->adapter.user_start  = 0x1d000000;
    a->adapter.user_nbytes = a->boot_start & 0x00ffffff;
    a->adapter.boot_nbytes = 12*1024 - a->erase_size;
    printf ("      Adapter: UHB Bootloader '%s', Version %x.%02x\n",
        a->name, a->version >> 8, a->version & 0xff);
    printf (" Program area: %08x-%08x, %08x-%08x\n", a->adapter.user_start,
        a->adapter.user_start + a->adapter.user_nbytes - 1,
        0x1fc00000, 0x1fc00000+ a->adapter.boot_nbytes - 1);

    if (debug_level > 0) {
        printf ("   Flash size: %u bytes\n", a->flash_size);
        printf ("  Write block: %u bytes\n", a->write_size);
        printf ("  Erase block: %u bytes\n", a->erase_size);
        printf ("   Boot start: %08x\n", a->boot_start);
    }

    /* Enter Bootloader mode. */
    uhb_command (a, CMD_BOOT, 0, 0, 0, 0);
    if (a->reply[0] != STX || a->reply[1] != CMD_BOOT) {
        fprintf (stderr, "uhb: Cannot enter bootloader mode.\n");
        return 0;
    }

    a->adapter.flags = (AD_PROBE | AD_ERASE | AD_READ | AD_WRITE);

    /* User functions. */
    a->adapter.close = uhb_close;
    a->adapter.get_idcode = uhb_get_idcode;
    a->adapter.read_word = uhb_read_word;
    a->adapter.verify_data = uhb_verify_data;
    a->adapter.erase_chip = uhb_erase_chip;
    a->adapter.program_block = uhb_program_block;
    a->adapter.program_word = uhb_program_word;
    return &a->adapter;
}
