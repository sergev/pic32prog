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
#include <usb.h>

#include "adapter.h"
#include "hidapi.h"
#include "pic32.h"

/* Bootloader commands */
#define CMD_QUERY_DEVICE        0x02
#define CMD_UNLOCK_CONFIG       0x03
#define CMD_ERASE_DEVICE        0x04
#define CMD_PROGRAM_DEVICE      0x05
#define CMD_PROGRAM_COMPLETE    0x06
#define CMD_GET_DATA            0x07
#define CMD_RESET_DEVICE        0x08

typedef struct {
    /* Common part */
    adapter_t adapter;

    /* Device handle for libusb. */
    hid_device *hiddev;

    unsigned char reply [64];
    int reply_len;

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
    unsigned char *data, unsigned nbytes)
{
    unsigned char buf [64];
    unsigned k;

    memset (buf, 0, sizeof(buf));
    buf[0] = cmd;
    if (nbytes > 0)
        memcpy (buf+1, data, nbytes);

    if (debug_level > 0) {
        fprintf (stderr, "---Send");
        for (k=0; k<=nbytes; ++k) {
            if (k != 0 && (k & 15) == 0)
                fprintf (stderr, "\n       ");
            fprintf (stderr, " %02x", buf[k]);
        }
        fprintf (stderr, "\n");
    }
    hid_write (a->hiddev, buf, 64);

    if (cmd != CMD_QUERY_DEVICE && cmd != CMD_GET_DATA) {
        /* No reply expected. */
        return;
    }

    memset (a->reply, 0, sizeof(a->reply));
    a->reply_len = hid_read_timeout (a->hiddev, a->reply, 64, 4000);
    if (a->reply_len == 0) {
        fprintf (stderr, "Timed out.\n");
        exit (-1);
    }
    if (a->reply_len != 64) {
        fprintf (stderr, "uhb: error %d receiving packet\n", a->reply_len);
        exit (-1);
    }
    if (debug_level > 0) {
        fprintf (stderr, "---Recv");
        for (k=0; k<a->reply_len; ++k) {
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
    if (power_on)
        uhb_command (a, CMD_RESET_DEVICE, 0, 0);
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
    /* TODO */
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

static void program_flash (uhb_adapter_t *a,
    unsigned addr, unsigned *data, unsigned nwords)
{
    unsigned char request[64];
    unsigned nbytes;

    //fprintf (stderr, "uhb: program %d bytes at %08x: %08x-%08x-...-%08x\n",
    //    nbytes, addr, data[0], data[1], data[nwords-1]);

    nbytes = nwords * 4;
    if (addr < a->adapter.user_start ||
        addr + nbytes >= a->adapter.user_start + a->adapter.user_nbytes)
    {
        fprintf (stderr, "address %08x out of program area\n", addr);
        return;
    }

    memset (request, 0, sizeof(request));
    *(unsigned*) &request[0] = addr;
    request[4] = nbytes;
    request[5] = 0;
    request[6] = 0;

    /* Data is right aligned. */
    memcpy (request + 63 - nbytes, data, nbytes);

    uhb_command (a, CMD_PROGRAM_DEVICE, request, 63);
}

/*
 * Flash write, 1-kbyte blocks.
 */
static void uhb_program_block (adapter_t *adapter,
    unsigned addr, unsigned *data)
{
    uhb_adapter_t *a = (uhb_adapter_t*) adapter;
    int nwords;

    for (nwords=256; nwords>0; nwords-=14) {
        /* 14 words = 56 bytes per packet. */
        program_flash (a, addr, data, nwords>14 ? 14 : nwords);
        data += 14;
        addr += 14*4;
    }
    uhb_command (a, CMD_PROGRAM_COMPLETE, 0, 0);
}

/*
 * Erase all flash memory.
 */
static void uhb_erase_chip (adapter_t *adapter)
{
    uhb_adapter_t *a = (uhb_adapter_t*) adapter;

    //fprintf (stderr, "uhb: erase chip\n");
    uhb_command (a, CMD_ERASE_DEVICE, 0, 0);

    /* To wait when erase finished, query a reply. */
    uhb_command (a, CMD_QUERY_DEVICE, 0, 0);
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
    uhb_command (a, CMD_QUERY_DEVICE, 0, 0);
    if (a->reply[0] != CMD_QUERY_DEVICE ||
        a->reply[1] != 56 ||                /* HID packet data size */
        a->reply[2] != 3 ||                 /* PIC32 device family */
        a->reply[3] != 1)                   /* program memory type */
            return 0;

    a->adapter.user_start = *(unsigned*) &a->reply[4] & 0x1fffffff;
    a->adapter.user_nbytes = *(unsigned*) &a->reply[8] & 0x0fffffff;
    printf ("      Adapter: UHB Bootloader\n");
    printf (" Program area: %08x-%08x\n", a->adapter.user_start,
        a->adapter.user_start + a->adapter.user_nbytes - 1);

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
