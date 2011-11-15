/*
 * Interface to PIC32 Microchip USB bootloader.
 *
 * Copyright (C) 2011 Serge Vakulenko
 *
 * This file is part of BKUNIX project, which is distributed
 * under the terms of the GNU General Public License (GPL).
 * See the accompanying file "COPYING" for more details.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <usb.h>

#include "adapter.h"
#include "hidapi.h"
#include "pic32.h"

#define FRAME_SOH           0x01
#define FRAME_EOT           0x04
#define FRAME_DLE           0x10

#define CMD_READ_VERSION    0x01
#define CMD_ERASE_FLASH     0x02
#define CMD_PROGRAM_FLASH   0x03
#define CMD_READ_CRC        0x04
#define CMD_JUMP_APP        0x05

typedef struct {
    /* Common part */
    adapter_t adapter;

    /* Device handle for libusb. */
    hid_device *hiddev;

    unsigned char reply [64];
    int reply_len;

} boot_adapter_t;

/*
 * Identifiers of USB adapter.
 */
#define MICROCHIP_VID           0x04d8
#define BOOTLOADER_PID          0x003c  /* Microchip HID Bootloader */

/*
 * USB endpoints.
 */
#define OUT_EP                  0x01
#define IN_EP                   0x81

#define TIMO_MSEC               1000

/*
 * Calculate checksum.
 */
static unsigned calculate_crc (unsigned crc, unsigned char *data, unsigned nbytes)
{
    static const unsigned short crc_table [16] = {
        0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7,
        0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef,
    };
    unsigned i;

    while (nbytes--) {
        i = (crc >> 12) ^ (*data >> 4);
        crc = crc_table[i & 0x0F] ^ (crc << 4);
        i = (crc >> 12) ^ (*data >> 0);
        crc = crc_table[i & 0x0F] ^ (crc << 4);
        data++;
    }
    return crc & 0xffff;
}

static void boot_send (hid_device *hiddev, unsigned char *buf, unsigned nbytes)
{
    if (debug_level > 0) {
        int k;
        fprintf (stderr, "---Send");
        for (k=0; k<nbytes; ++k) {
            if (k != 0 && (k & 15) == 0)
                fprintf (stderr, "\n       ");
            fprintf (stderr, " %02x", buf[k]);
        }
        fprintf (stderr, "\n");
    }
    hid_write (hiddev, buf, 64);
}

static int boot_recv (hid_device *hiddev, unsigned char *buf)
{
    int n;

    n = hid_read (hiddev, buf, 64);
    if (n <= 0) {
        fprintf (stderr, "hidboot: error %d receiving packet\n", n);
        exit (-1);
    }
    if (debug_level > 0) {
        int k;
        fprintf (stderr, "---Recv");
        for (k=0; k<n; ++k) {
            if (k != 0 && (k & 15) == 0)
                fprintf (stderr, "\n       ");
            fprintf (stderr, " %02x", buf[k]);
        }
        fprintf (stderr, "\n");
    }
    return n;
}

static inline unsigned add_byte (unsigned char c,
    unsigned char *buf, unsigned indx)
{
    if (c == FRAME_EOT || c == FRAME_SOH || c == FRAME_DLE)
        buf[indx++] = FRAME_DLE;
    buf[indx++] = c;
    return indx;
}

/*
 * Send a request to the device.
 * Store the reply into the a->reply[] array.
 */
static void boot_command (boot_adapter_t *a, unsigned char cmd,
    unsigned char *data, unsigned data_len)
{
    unsigned char buf [64];
    unsigned i, n, c, crc;

    if (debug_level > 0) {
        int k;
        fprintf (stderr, "---Cmd%d", cmd);
        for (k=0; k<data_len; ++k) {
            if (k != 0 && (k & 15) == 0)
                fprintf (stderr, "\n       ");
            fprintf (stderr, " %02x", data[k]);
        }
        fprintf (stderr, "\n");
    }
    memset (buf, FRAME_EOT, sizeof(buf));
    n = 0;
    buf[n++] = FRAME_SOH;

    n = add_byte (cmd, buf, n);
    crc = calculate_crc (0, &cmd, 1);

    if (data_len > 0) {
        for (i=0; i<data_len; ++i)
            n = add_byte (data[i], buf, n);
        crc = calculate_crc (crc, data, data_len);
    }
    n = add_byte (crc, buf, n);
    n = add_byte (crc >> 8, buf, n);

    buf[n++] = FRAME_EOT;
    boot_send (a->hiddev, buf, n);

    if (cmd == CMD_JUMP_APP) {
        /* No reply expected. */
        return;
    }
    n = boot_recv (a->hiddev, buf);
    c = 0;
    for (i=0; i<n; ++i) {
        switch (buf[i]) {
        default:
            a->reply[c++] = buf[i];
            continue;
        case FRAME_DLE:
            a->reply[c++] = buf[++i];
            continue;
        case FRAME_SOH:
            c = 0;
            continue;
        case FRAME_EOT:
            a->reply_len = 0;
            if (c > 2) {
                unsigned crc = a->reply[c-2] | (a->reply[c-1] << 8);
                if (crc == calculate_crc (0, a->reply, c-2))
                    a->reply_len = c - 2;
            }
            if (a->reply_len > 0 && debug_level > 0) {
                int k;
                fprintf (stderr, "--->>>>");
                for (k=0; k<a->reply_len; ++k) {
                    if (k != 0 && (k & 15) == 0)
                        fprintf (stderr, "\n       ");
                    fprintf (stderr, " %02x", a->reply[k]);
                }
                fprintf (stderr, "\n");
            }
            return;
        }
    }
}

static void boot_close (adapter_t *adapter, int power_on)
{
    boot_adapter_t *a = (boot_adapter_t*) adapter;
    //fprintf (stderr, "hidboot: close\n");

    free (a);
}

/*
 * Return the Device Identification code
 */
static unsigned boot_get_idcode (adapter_t *adapter)
{
    /* Assume 795F512L. */
    return 0x04307053;
}

/*
 * Read a word from memory (without PE).
 */
static unsigned boot_read_word (adapter_t *adapter, unsigned addr)
{
//    boot_adapter_t *a = (boot_adapter_t*) adapter;
/*TODO*/
    return 0;
}

/*
 * Read a block of memory, multiple of 1 kbyte.
 */
static void boot_read_data (adapter_t *adapter,
    unsigned addr, unsigned nwords, unsigned *data)
{
    //fprintf (stderr, "hidboot: read %d bytes from %08x\n", nwords*4, addr);
    for (; nwords > 0; nwords--) {
        *data++ = boot_read_word (adapter, addr);
        addr += 4;
    }
}

/*
 * Write a word to flash memory.
 */
static void boot_program_word (adapter_t *adapter,
    unsigned addr, unsigned word)
{
//    boot_adapter_t *a = (boot_adapter_t*) adapter;

    if (debug_level > 0)
        fprintf (stderr, "hidboot: program word at %08x: %08x\n", addr, word);
/*TODO*/
}

/*
 * Flash write, 1-kbyte blocks.
 */
static void boot_program_block (adapter_t *adapter,
    unsigned addr, unsigned *data)
{
//    boot_adapter_t *a = (boot_adapter_t*) adapter;
    unsigned nwords = 256;

    if (debug_level > 0)
        fprintf (stderr, "hidboot: program %d bytes at %08x\n", nwords*4, addr);
/*TODO*/
}

/*
 * Erase all flash memory.
 */
static void boot_erase_chip (adapter_t *adapter)
{
//    boot_adapter_t *a = (boot_adapter_t*) adapter;

    //fprintf (stderr, "hidboot: erase chip\n");
/*TODO*/
}

/*
 * Initialize adapter hidboot.
 * Return a pointer to a data structure, allocated dynamically.
 * When adapter not found, return 0.
 */
adapter_t *adapter_open_boot (void)
{
    boot_adapter_t *a;
    hid_device *hiddev;

    hiddev = hid_open (MICROCHIP_VID, BOOTLOADER_PID, 0);
    if (! hiddev) {
        /*fprintf (stderr, "HID bootloader not found: vid=%04x, pid=%04x\n",
            MICROCHIP_VID, BOOTLOADER_PID);*/
        return 0;
    }
    a = calloc (1, sizeof (*a));
    if (! a) {
        fprintf (stderr, "Out of memory\n");
        return 0;
    }
    a->hiddev = hiddev;

    /* Read version of adapter. */
    boot_command (a, CMD_READ_VERSION, 0, 0);
    printf ("      Adapter: HID Bootloader Version %d.%d\n",
        a->reply[1], a->reply[2]);

    /* User functions. */
    a->adapter.close = boot_close;
    a->adapter.get_idcode = boot_get_idcode;
    a->adapter.read_word = boot_read_word;
    a->adapter.read_data = boot_read_data;
    a->adapter.erase_chip = boot_erase_chip;
    a->adapter.program_block = boot_program_block;
    a->adapter.program_word = boot_program_word;
    return &a->adapter;
}
