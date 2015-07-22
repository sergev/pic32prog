/*
 * Interface to PIC32 bootloader with serial STK500v2 protocol.
 *
 * Copyright (C) 2014-2015 Serge Vakulenko
 *
 * This file is part of PIC32PROG project, which is distributed
 * under the terms of the GNU General Public License (GPL).
 * See the accompanying file "COPYING" for more details.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "adapter.h"
#include "pic32.h"
#include "serial.h"

/*
 * AVR068 - STK500 Communication Protocol
 */

/* STK message constants */
#define MESSAGE_START           0x1B    /* ESC = 27 decimal */
#define TOKEN                   0x0E

/* STK command constants */
#define CMD_SIGN_ON             0x01
#define CMD_LOAD_ADDRESS        0x06
#define CMD_ENTER_PROGMODE_ISP  0x10
#define CMD_LEAVE_PROGMODE_ISP  0x11
#define CMD_CHIP_ERASE_ISP      0x12
#define CMD_PROGRAM_FLASH_ISP   0x13
#define CMD_READ_FLASH_ISP      0x14
#define CMD_SPI_MULTI           0x1D
#define CMD_SET_PARAMETER       0x40
#define CMD_GET_PARAMETER       0x41
#define CMD_SET_BAUD            0x48

/* STK status constants */
#define STATUS_CMD_OK           0x00    /* Success */

#define PAGE_NBYTES             256     /* Use 256-byte packets. */

unsigned int adapter_baud;

typedef struct {
    /* Common part */
    adapter_t adapter;

    unsigned char   sequence_number;
    unsigned char   page_addr_fetched;
    unsigned        page_addr;
    unsigned        last_load_addr;
    unsigned char   page [PAGE_NBYTES];

} stk_adapter_t;

/*
 * Send the command sequence and get back a response.
 */
static int send_receive (stk_adapter_t *a, unsigned char *cmd, int cmdlen,
    unsigned char *response, int reply_len)
{
    unsigned char *p, sum, hdr [5];
    int len, i, got, rlen, retry = 0;
again:
    /*
     * Prepare header and checksum.
     */
    hdr[0] = MESSAGE_START;
    hdr[1] = ++a->sequence_number;
    hdr[2] = cmdlen >> 8;
    hdr[3] = cmdlen;
    hdr[4] = TOKEN;
    sum = hdr[0] ^ hdr[1] ^ hdr[2] ^ hdr[3] ^ hdr[4];
    for (i=0; i<cmdlen; ++i)
        sum ^= cmd[i];

    /*
     * Send command.
     */
    if (debug_level > 1) {
        printf ("send [%d] %x-%x-%x-%x-%x",
            5 + cmdlen + 1, hdr[0], hdr[1], hdr[2], hdr[3], hdr[4]);
        for (i=0; i<cmdlen; ++i)
            printf ("-%x", cmd[i]);
        printf ("-%x\n", sum);
    }

    if (serial_write (hdr, 5) < 0 ||
        serial_write (cmd, cmdlen) < 0 ||
        serial_write (&sum, 1) < 0) {
        fprintf (stderr, "stk-send: write error\n");
        exit (-1);
    }

    /*
     * Get header.
     */
    p = hdr;
    len = 0;
    while (len < 5) {
        got = serial_read (p, 5 - len);
        if (! got)
            return 0;

        p += got;
        len += got;
    }
    if (hdr[0] != MESSAGE_START || hdr[1] != a->sequence_number ||
        hdr[4] != TOKEN) {
        /* Skip all incoming data. */
        unsigned char buf [300];

        if (retry)
            printf ("got invalid header: %x-%x-%x-%x-%x\n",
                hdr[0], hdr[1], hdr[2], hdr[3], hdr[4]);
flush_input:
        serial_read (buf, sizeof (buf));
        if (retry) {
            retry = 1;
            goto again;
        }
        return 0;
    }
    rlen = hdr[2] << 8 | hdr[3];
    if (rlen == 0 || rlen > reply_len) {
        printf ("invalid reply length=%d, expecting %d bytes\n",
            rlen, reply_len);
        goto flush_input;
    }

    /*
     * Get response.
     */
    p = response;
    len = 0;
    while (len < rlen) {
        got = serial_read (p, rlen - len);
        if (! got)
            return 0;

        p += got;
        len += got;
    }

    /*
     * Get sum.
     */
    p = &sum;
    len = 0;
    while (len < 1) {
        got = serial_read (p, 1);
        if (! got)
            return 0;
        ++len;
    }

    if (debug_level > 1) {
        printf (" got [%d] %x-%x-%x-%x-%x",
            5 + rlen + 1, hdr[0], hdr[1], hdr[2], hdr[3], hdr[4]);
        for (i=0; i<rlen; ++i)
            printf ("-%x", response[i]);
        printf ("-%x\n", sum);
    }

    /* Check sum. */
    sum ^= hdr[0] ^ hdr[1] ^ hdr[2] ^ hdr[3] ^ hdr[4];
    for (i=0; i<rlen; ++i)
        sum ^= response[i];
    if (sum != 0) {
        printf ("invalid reply checksum\n");
        goto flush_input;
    }
    return 1;
}

extern unsigned int alternate_speed;

static void switch_baud(stk_adapter_t *a) {
    if (alternate_speed != adapter_baud) {

        unsigned char cmd [5] = { CMD_SET_BAUD,
            (alternate_speed) & 0xFF,
            (alternate_speed >> 8) & 0xFF,
            (alternate_speed >> 16) & 0xFF,
            (alternate_speed >> 24) & 0xFF,
        };
        unsigned char response [6];

        if (! send_receive (a, cmd, 5, response, 6) || response[0] != cmd[0] ||
            response[1] != STATUS_CMD_OK) {
                printf("    Baud rate: %d bps\n", adapter_baud);
        } else {
            if ((response[2] == (alternate_speed & 0xFF)) &&
                (response[3] == ((alternate_speed >> 8) & 0xFF)) &&
                (response[4] == ((alternate_speed >> 16) & 0xFF)) &&
                (response[5] == ((alternate_speed >> 24) & 0xFF))) {
                serial_baud(alternate_speed);
                printf("    Baud rate: %d bps\n", alternate_speed);
            } else {
                printf("    Baud rate: %d bps\n", adapter_baud);
            }
        }
    }
}

static void prog_enable (stk_adapter_t *a)
{
    unsigned char cmd [12] = { CMD_ENTER_PROGMODE_ISP,
        200,    /* timeout in msec */
        100,    /* pin stabilization delay in msec */
        25,     /* command execution delay in msec */
        32,     /* number of synchronization loops */
        0,      /* per byte delay */
        0x53,   /* poll value, 53h for AVR, 69h for AT89xx */
        3,      /* poll index, 3 for AVR, 4 for AT89xx */
        0xAC, 0x53, 0x00, 0x00
    };
    unsigned char response [2];

    if (! send_receive (a, cmd, 12, response, 2) || response[0] != cmd[0] ||
        response[1] != STATUS_CMD_OK) {
        fprintf (stderr, "Cannot enter programming mode.\n");
        exit (-1);
    }
}

static void chip_erase (stk_adapter_t *a)
{
    unsigned char cmd [7] = { CMD_CHIP_ERASE_ISP,
        150,                    /* erase delay */
        0,                      /* poll method */
        0xAC, 0x80, 0x00, 0x00  /* command */
    };
    unsigned char response [2];

    if (! send_receive (a, cmd, 7, response, 2) || response[0] != cmd[0] ||
        response[1] != STATUS_CMD_OK) {
        fprintf (stderr, "Cannot erase chip.\n");
        exit (-1);
    }
}

static void prog_disable (stk_adapter_t *a)
{
    unsigned char cmd [3] = { CMD_LEAVE_PROGMODE_ISP,
        1,      /* pre-delay in msec */
        1,      /* post-delay in msec */
    };
    unsigned char response [2];

    /* Skip all incoming data. */
    unsigned char buf [300];
    serial_read (buf, sizeof (buf));

    /* Leave programming mode; ignore errors. */
    send_receive (a, cmd, 3, response, 2);
}

static void load_address (stk_adapter_t *a, unsigned addr)
{
    unsigned char cmd [5] = { CMD_LOAD_ADDRESS,
        0, (addr >> 16) & 0x7f, addr >> 8, addr, };
    unsigned char response [2];

    if (a->last_load_addr == addr)
        return;

    if (debug_level > 1)
        printf ("Load address: %#x\n", addr & 0x7f0000);

    if (! send_receive (a, cmd, 5, response, 2) || response[0] != cmd[0] ||
        response[1] != STATUS_CMD_OK) {
        fprintf (stderr, "Load address failed.\n");
        exit (-1);
    }
    a->last_load_addr = addr;
}

static void flush_write_buffer (stk_adapter_t *a)
{
    unsigned char cmd [10+PAGE_NBYTES] = { CMD_PROGRAM_FLASH_ISP,
        PAGE_NBYTES >> 8, PAGE_NBYTES & 0xff, 0, 0, 0, 0, 0, 0, 0 };
    unsigned char response [2];

    if (! a->page_addr_fetched)
        return;

    load_address (a, a->page_addr >> 1);

    if (debug_level > 1)
        printf ("Programming page: %#x\n", a->page_addr);
    memcpy (cmd+10, a->page, PAGE_NBYTES);
    if (! send_receive (a, cmd, 10+PAGE_NBYTES, response, 2) ||
        response[0] != cmd[0]) {
        fprintf (stderr, "Program flash failed.\n");
        exit (-1);
    }
    if (response[1] != STATUS_CMD_OK)
        printf ("Programming flash: timeout at %#x\n", a->page_addr);

    a->page_addr_fetched = 0;
    a->last_load_addr += PAGE_NBYTES / 2;
}

/*
 * PAGE MODE PROGRAMMING:
 * Cache page address. When current address is out of the page address,
 * flush page buffer and continue programming.
 */
static void write_byte (stk_adapter_t *a, unsigned addr, unsigned char byte)
{
    if (debug_level > 2)
        printf ("Loading to address: %#x (page_addr_fetched=%s)\n",
            addr, a->page_addr_fetched ? "Yes" : "No");

    if (a->page_addr / PAGE_NBYTES != addr / PAGE_NBYTES)
        flush_write_buffer (a);

    if (! a->page_addr_fetched) {
        a->page_addr = addr / PAGE_NBYTES * PAGE_NBYTES;
        a->page_addr_fetched = 1;
    }
    a->page [addr % PAGE_NBYTES] = byte;
}

static void read_page (stk_adapter_t *a, unsigned addr,
    unsigned char *buf)
{
    unsigned char cmd [4] = { CMD_READ_FLASH_ISP, 1, 0, 0x20 };
    unsigned char response [3+PAGE_NBYTES];

    load_address (a, addr >> 1);
    if (debug_level > 1)
        printf ("Read page: %#x\n", addr);

    if (! send_receive (a, cmd, 4, response, 3+PAGE_NBYTES) ||
        response[0] != cmd[0] ||
        response[1] != STATUS_CMD_OK ||
        response[2+PAGE_NBYTES] != STATUS_CMD_OK) {
        fprintf (stderr, "Read page failed.\n");
        exit (-1);
    }
    memcpy (buf, response+2, PAGE_NBYTES);
    a->last_load_addr += PAGE_NBYTES / 2;
}

static void stk_close (adapter_t *adapter, int power_on)
{
    stk_adapter_t *a = (stk_adapter_t*) adapter;

    prog_disable (a);

    /* restore and close serial port */
    serial_close();
    free (a);
}

/*
 * Return the Device Identification code
 */
static unsigned stk_get_idcode (adapter_t *adapter)
{
    /* Bootloader does not allow to get cpu ID code. */
    return 0xDEAFB00B;
}

/*
 * Read a configuration word from memory.
 */
static unsigned stk_read_word (adapter_t *adapter, unsigned addr)
{
    stk_adapter_t *a = (stk_adapter_t*) adapter;
    unsigned char cmd [4] = { CMD_READ_FLASH_ISP, 0, 4, 0x20 };
    unsigned char response [7];

    load_address (a, addr >> 1);
    if (! send_receive (a, cmd, 4, response, 7) || response[0] != cmd[0] ||
        response[1] != STATUS_CMD_OK || response[6] != STATUS_CMD_OK) {
        fprintf (stderr, "Read word failed.\n");
        exit (-1);
    }
    return response[2] | response[3] << 8 |
        response[4] << 16 | response[5] << 24;
}

/*
 * Write a configuration word to flash memory.
 */
static void stk_program_word (adapter_t *adapter,
    unsigned addr, unsigned word)
{
    /* This function is needed for programming DEVCFG fuses.
     * Not supported by booloader. */
    if (debug_level > 0)
        fprintf (stderr, "stk: program word at %08x: %08x\n", addr, word);
}

/*
 * Erase all flash memory.
 */
static void stk_erase_chip (adapter_t *adapter)
{
    stk_adapter_t *a = (stk_adapter_t*) adapter;

    chip_erase (a);
    prog_enable (a);
}

/*
 * Verify a block of memory (1024 bytes).
 */
static void stk_verify_data (adapter_t *adapter,
    unsigned addr, unsigned nwords, unsigned *data)
{
    stk_adapter_t *a = (stk_adapter_t*) adapter;
    unsigned block [PAGE_NBYTES], i, expected, word;

    /* Read block of data. */
    for (i=0; i<1024; i+=PAGE_NBYTES)
        read_page (a, addr+i, i + (unsigned char*) block);

    /* Compare. */
    for (i=0; i<nwords; i++) {
        expected = data [i];
        word = block [i];
        if (word != expected) {
            printf ("\nerror at address %08X: file=%08X, mem=%08X\n",
                addr + i*4, expected, word);
            exit (1);
        }
    }
}

/*
 * Flash write, 1-kbyte blocks.
 */
static void stk_program_block (adapter_t *adapter,
    unsigned addr, unsigned *data)
{
    stk_adapter_t *a = (stk_adapter_t*) adapter;
    unsigned i;

    for (i=0; i<PAGE_NBYTES; ++i) {
        write_byte (a, addr + i*4,     data[i]);
        write_byte (a, addr + i*4 + 1, data[i] >> 8);
        write_byte (a, addr + i*4 + 2, data[i] >> 16);
        write_byte (a, addr + i*4 + 3, data[i] >> 24);
    }
    flush_write_buffer (a);
}

adapter_t *adapter_open_stk500v2 (const char *port, int baud_rate)
{
    stk_adapter_t *a;

    adapter_baud = baud_rate;

    a = calloc (1, sizeof (*a));
    if (! a) {
        fprintf (stderr, "Out of memory\n");
        return 0;
    }

    /* Open serial port */
    if (serial_open (port, baud_rate, 1000) < 0) {
        /* failed to open serial port */
        free (a);
        return 0;
    }
    usleep (200000);

    /* Detect the device. */
    int retry_count;
    unsigned char response [12];

    /* Synchronize */
    retry_count = 0;
    int outer_retry = 0;
    for (;;) {
        /* Send CMD_SIGN_ON. */
        if (send_receive (a, (unsigned char*)"\1", 1, response, 11) &&
            memcmp (response, "\1\0\10STK500_2", 11) == 0) {
            if (debug_level > 1)
                printf ("stk-probe: OK\n");
            break;
        }
        ++retry_count;
        if (debug_level > 1) {
            printf ("stk-probe: retry %d: "
                "%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x\n",
                retry_count, response[0], response[1], response[2],
                response[3], response[4], response[5], response[6],
                response[7], response[8], response[9], response[10]);
        }
        if (retry_count >= 3) {
            /* Bad reply or no device connected */
            retry_count = 0;
            serial_close();
            usleep(200000);
            serial_open(port, baud_rate, 1000);
            outer_retry++;
        }
        if (outer_retry >= 2) {
            free (a);
            serial_close ();
            return 0;
        }
    }

    switch_baud(a);

    prog_enable (a);
    a->last_load_addr = -1;

    /* Identify device. */
    printf ("      Adapter: STK500v2 Bootloader\n");

    a->adapter.user_start = 0x1d000000;
    a->adapter.user_nbytes = 2048 * 1024;
    a->adapter.boot_nbytes = 80 * 1024;
    a->adapter.flags = (AD_PROBE | AD_ERASE | AD_READ | AD_WRITE);

    printf (" Program area: %08x-%08x\n", a->adapter.user_start,
        a->adapter.user_start + a->adapter.user_nbytes - 1);

    /* User functions. */
    a->adapter.close = stk_close;
    a->adapter.get_idcode = stk_get_idcode;
    a->adapter.read_word = stk_read_word;
    a->adapter.verify_data = stk_verify_data;
    a->adapter.program_block = stk_program_block;
    a->adapter.program_word = stk_program_word;
    a->adapter.erase_chip = stk_erase_chip;
    return &a->adapter;
}
