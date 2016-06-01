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
#define CMD_SET_PARAMETER       0x02
#define CMD_GET_PARAMETER       0x03
#define CMD_LOAD_ADDRESS        0x06
#define CMD_ENTER_PROGMODE_ISP  0x10
#define CMD_LEAVE_PROGMODE_ISP  0x11
#define CMD_CHIP_ERASE_ISP      0x12
#define CMD_PROGRAM_FLASH_ISP   0x13
#define CMD_READ_FLASH_ISP      0x14
#define CMD_SPI_MULTI           0x1D
#define CMD_SET_BAUD            0x48

#define PARAM_CK_VEND_LOW       0x40
#define PARAM_CK_VEND_HIGH      0x41
#define PARAM_CK_PROD_LOW       0x42
#define PARAM_CK_PROD_HIGH      0x43
#define PARAM_CK_DEVID_LOW      0x44
#define PARAM_CK_DEVID_MID      0x45
#define PARAM_CK_DEVID_HIGH     0x46
#define PARAM_CK_DEVID_TOP      0x47
#define PARAM_CK_DEVID_REV      0x48

/* STK status constants */
#define STATUS_CMD_OK           0x00    /* Success */

#define PAGE_NBYTES             256     /* Write packet sise */
#define READ_NBYTES             256     /* Read packet size */

extern unsigned int alternate_speed;

typedef struct {
    /* Common part */
    adapter_t adapter;

    int             first_time;
    int             timeout_msec;
    unsigned        baud;
    unsigned char   sequence_number;
    unsigned char   page_addr_fetched;
    unsigned        page_addr;
    unsigned        last_load_addr;
    unsigned char   page [PAGE_NBYTES];

} stk_adapter_t;

/*
 * Send the command sequence and get back a response.
 */
static int send_receive(stk_adapter_t *a, unsigned char *cmd, int cmdlen,
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
        printf("send [%d] %x-%x-%x-%x-%x",
            5 + cmdlen + 1, hdr[0], hdr[1], hdr[2], hdr[3], hdr[4]);
        for (i=0; i<cmdlen; ++i)
            printf("-%x", cmd[i]);
        printf("-%x\n", sum);
    }

    if (serial_write(hdr, 5) < 0 ||
        serial_write(cmd, cmdlen) < 0 ||
        serial_write(&sum, 1) < 0) {
        fprintf(stderr, "stk-send: write error\n");
        exit(-1);
    }

    /*
     * Get header.
     */
    p = hdr;
    len = 0;
    while (len < 5) {
        got = serial_read(p, 5 - len, a->timeout_msec);
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
            printf("got invalid header: %x-%x-%x-%x-%x\n",
                hdr[0], hdr[1], hdr[2], hdr[3], hdr[4]);
flush_input:
        serial_read(buf, sizeof(buf), a->timeout_msec);
        if (retry) {
            retry = 1;
            goto again;
        }
        return 0;
    }
    rlen = hdr[2] << 8 | hdr[3];
    if (rlen == 0 || rlen > reply_len) {
        printf("invalid reply length=%d, expecting %d bytes\n",
            rlen, reply_len);
        goto flush_input;
    }

    /*
     * Get response.
     */
    p = response;
    len = 0;
    while (len < rlen) {
        got = serial_read(p, rlen - len, a->timeout_msec);
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
        got = serial_read(p, 1, a->timeout_msec);
        if (! got)
            return 0;
        ++len;
    }

    if (debug_level > 1) {
        printf(" got [%d] %x-%x-%x-%x-%x",
            5 + rlen + 1, hdr[0], hdr[1], hdr[2], hdr[3], hdr[4]);
        for (i=0; i<rlen; ++i)
            printf("-%x", response[i]);
        printf("-%x\n", sum);
    }

    /* Check sum. */
    sum ^= hdr[0] ^ hdr[1] ^ hdr[2] ^ hdr[3] ^ hdr[4];
    for (i=0; i<rlen; ++i)
        sum ^= response[i];
    if (sum != 0) {
        printf("invalid reply checksum\n");
        goto flush_input;
    }
    return 1;
}

static void switch_baud(stk_adapter_t *a)
{
    unsigned char cmd [5] = { CMD_SET_BAUD,
        (alternate_speed) & 0xFF,
        (alternate_speed >> 8) & 0xFF,
        (alternate_speed >> 16) & 0xFF,
        (alternate_speed >> 24) & 0xFF,
    };
    unsigned char response [6];

    if (alternate_speed != a->baud) {
        if (send_receive(a, cmd, 5, response, 6) &&
            response[0] == cmd[0] &&
            response[1] == STATUS_CMD_OK &&
            response[2] == cmd[1] &&
            response[3] == cmd[2] &&
            response[4] == cmd[3] &&
            response[5] == cmd[4])
        {
            serial_baud(alternate_speed);
            printf("    Baud rate: %d bps\n", alternate_speed);
        } else {
            printf("    Baud rate: %d bps\n", a->baud);
        }
    }
}

static unsigned char get_parameter(stk_adapter_t *a, unsigned char param) {
    unsigned char cmd [2] = { CMD_GET_PARAMETER, param };
    unsigned char response [3];

    if (debug_level > 1)
        printf("Get parameter %x\n", param);

    if (! send_receive(a, cmd, 2, response, 3) || response[0] != cmd[0] || response[1] != STATUS_CMD_OK) {
        fprintf(stderr, "Error fetching parameter %d\n", param);
        exit(-1);
    }
    if (debug_level > 1)
        printf("Value %x\n", response[2]);
    return response[2];
}

static void set_parameter(stk_adapter_t *a, unsigned char param, int val) {
    unsigned char cmd [3] = { CMD_SET_PARAMETER, param, val };
    unsigned char response [2];

    if (debug_level > 1)
        printf("Set parameter %x\n", param);

    if (! send_receive(a, cmd, 3, response, 2) || response[0] != cmd[0] || response[1] != STATUS_CMD_OK) {
        fprintf(stderr, "Error setting parameter %d\n", param);
        exit(-1);
    }
}

static void prog_enable(stk_adapter_t *a)
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

    if (! send_receive(a, cmd, 12, response, 2) || response[0] != cmd[0] ||
        response[1] != STATUS_CMD_OK) {
        fprintf(stderr, "Cannot enter programming mode.\n");
        exit(-1);
    }
}

/*
 * This function does not actually do anything on the PIC32 side for
 * any known STK500 bootloader on the PIC32 (i.e. chipKIT). In addition,
 * a bug in an early bootloader version does not respond when this
 * command is sent. Thus PIC32Prog will fail to erase (and thus fail
 * to program) any board with this early bootloader version.
 * By simply retruning, PIC32Prog can be made to work with the early
 * bootloader.
 */
static void chip_erase(stk_adapter_t *a)
{
    /* Empty. */
}

static void prog_disable(stk_adapter_t *a)
{
    unsigned char cmd [3] = { CMD_LEAVE_PROGMODE_ISP,
        1,      /* pre-delay in msec */
        1,      /* post-delay in msec */
    };
    unsigned char response [2];

    /* Skip all incoming data. */
    unsigned char buf [300];
    serial_read(buf, sizeof(buf), a->timeout_msec);

    /* Leave programming mode; ignore errors. */
    send_receive(a, cmd, 3, response, 2);
}

static void load_address(stk_adapter_t *a, unsigned addr)
{
    // Convert an absolute address into a flash relative address
    if (addr >= (0x1D000000 >> 1)) {
        if (debug_level > 2)
            printf("Adjusting address 0x%08x to ", addr << 1);
        addr -= (0x1D000000 >> 1);
        if (debug_level > 2)
            printf("0x%08x\n", addr << 1);
    }

    unsigned char cmd [5] = { CMD_LOAD_ADDRESS,
        (addr >> 24), (addr >> 16), addr >> 8, addr, };
    unsigned char response [2];

    if (a->last_load_addr == addr)
        return;

    if (debug_level > 1)
        printf("Load address: %#x\n", addr << 1); // & 0x7f0000);

    if (! send_receive(a, cmd, 5, response, 2) || response[0] != cmd[0] ||
        response[1] != STATUS_CMD_OK) {
        fprintf(stderr, "Load address failed.\n");
        exit(-1);
    }
    a->last_load_addr = addr;
}

static void flush_write_buffer(stk_adapter_t *a)
{
    unsigned char cmd [10+PAGE_NBYTES] = { CMD_PROGRAM_FLASH_ISP,
        PAGE_NBYTES >> 8, PAGE_NBYTES & 0xff, 0, 0, 0, 0, 0, 0, 0 };
    unsigned char response [2];

    if (! a->page_addr_fetched)
        return;

    load_address(a, a->page_addr >> 1);

    /*
     * An early chipKIT bootloader version does a whole-chip erase
     * after the first CMD_PROGRAM_FLASH_ISP command, which can
     * take up to 4 seconds to complete. Thus the timeout needs
     * to be at least this long so that PIC32Prog will not timeout
     * during the erase cycle on that version of bootloader.
     */
    if (a->first_time) {
        a->timeout_msec = 5000;
        a->first_time = 0;
    } else {
        a->timeout_msec = 1000;
    }

    if (debug_level > 1)
        printf("Programming page: %#x\n", a->page_addr);
    memcpy(cmd+10, a->page, PAGE_NBYTES);
    if (! send_receive(a, cmd, 10+PAGE_NBYTES, response, 2) ||
        response[0] != cmd[0]) {
        fprintf(stderr, "Program flash failed.\n");
        exit(-1);
    }
    if (response[1] != STATUS_CMD_OK)
        printf("Programming flash: timeout at %#x\n", a->page_addr);

    a->page_addr_fetched = 0;
    a->last_load_addr += PAGE_NBYTES / 2;
}

/*
 * PAGE MODE PROGRAMMING:
 * Cache page address. When current address is out of the page address,
 * flush page buffer and continue programming.
 */
static void write_byte(stk_adapter_t *a, unsigned addr, unsigned char byte)
{
    if (debug_level > 2)
        printf("Loading to address: %#x (page_addr_fetched=%s)\n",
            addr, a->page_addr_fetched ? "Yes" : "No");

    if (a->page_addr / PAGE_NBYTES != addr / PAGE_NBYTES)
        flush_write_buffer(a);

    if (! a->page_addr_fetched) {
        a->page_addr = addr / PAGE_NBYTES * PAGE_NBYTES;
        a->page_addr_fetched = 1;
    }
    a->page [addr % PAGE_NBYTES] = byte;
}

/*
 * Read 256 bytes from the flash memory.
 * For some reason, the chipKIT bootloader fails to read blocks
 * shorter that 256 bytes.
 */
static void read_page(stk_adapter_t *a, unsigned addr,
    unsigned char *buf)
{
    unsigned char cmd [4] = { CMD_READ_FLASH_ISP,
        READ_NBYTES >> 8, READ_NBYTES & 0xff, 0x20 };
    unsigned char response [3+READ_NBYTES];

    load_address(a, addr >> 1);
    if (debug_level > 1)
        printf("Read page: %#x\n", addr);

    if (! send_receive(a, cmd, 4, response, 3+READ_NBYTES) ||
        response[0] != cmd[0] ||
        response[1] != STATUS_CMD_OK ||
        response[2+READ_NBYTES] != STATUS_CMD_OK) {
        fprintf(stderr, "Read page failed.\n");
        exit(-1);
    }
    memcpy(buf, response+2, READ_NBYTES);
    a->last_load_addr += READ_NBYTES / 2;
}

static void stk_close(adapter_t *adapter, int power_on)
{
    stk_adapter_t *a = (stk_adapter_t*) adapter;

    prog_disable(a);

    /* restore and close serial port */
    serial_close();
    free(a);
}

/*
 * Return the Device Identification code
 */
static unsigned stk_get_idcode(adapter_t *adapter)
{
    unsigned int i = 0;

    // A non-DEVID aware bootloader will just store
    // the parameters we send in the slot we specify. So
    // let's set the DEAFBOOB ID into the parameter
    // slots that correspond to the device ID.  A DEVID
    // aware bootloader will overwrite these with the
    // real device ID, so we can know if it's supported
    // or not.

    set_parameter((stk_adapter_t *)adapter, PARAM_CK_DEVID_LOW, 0x0B);
    set_parameter((stk_adapter_t *)adapter, PARAM_CK_DEVID_MID, 0xB0);
    set_parameter((stk_adapter_t *)adapter, PARAM_CK_DEVID_HIGH, 0xAF);
    set_parameter((stk_adapter_t *)adapter, PARAM_CK_DEVID_TOP, 0xDE);

    i = get_parameter((stk_adapter_t *)adapter, PARAM_CK_DEVID_LOW);
    i |= (get_parameter((stk_adapter_t *)adapter, PARAM_CK_DEVID_MID) << 8);
    i |= (get_parameter((stk_adapter_t *)adapter, PARAM_CK_DEVID_HIGH) << 16);
    i |= (get_parameter((stk_adapter_t *)adapter, PARAM_CK_DEVID_TOP) << 24);

    if (i == 0) {
        /* Bootloader does not allow to get cpu ID code. */
        if (debug_level > 1)
            printf("Cannot get the DEVID for the target\n");
        return 0xDEAFB00B;
    }
    return i;
}

/*
 * Read a configuration word from memory.
 */
static unsigned stk_read_word(adapter_t *adapter, unsigned addr)
{
    if (debug_level > 1)
        printf("Reading word from %x\n", addr);
    stk_adapter_t *a = (stk_adapter_t*) adapter;
    unsigned char cmd [4] = { CMD_READ_FLASH_ISP, 0, 4, 0x20 };
    unsigned char response [7];

    load_address(a, addr >> 1);
    if (debug_level > 1)
        printf("Sending read request\n");
    if (! send_receive(a, cmd, 4, response, 7) || response[0] != cmd[0] ||
        response[1] != STATUS_CMD_OK || response[6] != STATUS_CMD_OK) {
        fprintf(stderr, "Read word failed.\n");
        exit(-1);
    }
    if (debug_level > 1)
        printf("Read request done\n");
    return response[2] | response[3] << 8 |
        response[4] << 16 | response[5] << 24;
}

/*
 * Write a configuration word to flash memory.
 */
static void stk_program_word(adapter_t *adapter,
    unsigned addr, unsigned word)
{
    /* This function is needed for programming DEVCFG fuses.
     * Not supported by booloader. */
    if (debug_level > 0)
        fprintf(stderr, "stk: program word at %08x: %08x\n", addr, word);
}

/*
 * Erase all flash memory.
 */
static void stk_erase_chip(adapter_t *adapter)
{
    stk_adapter_t *a = (stk_adapter_t*) adapter;

    chip_erase(a);
    prog_enable(a);
}

/*
 * Verify a block of memory (1024 bytes).
 */
static void stk_verify_data(adapter_t *adapter,
    unsigned addr, unsigned nwords, unsigned *data)
{
    stk_adapter_t *a = (stk_adapter_t*) adapter;
    unsigned block [1024/4], i, expected, word;

    /* Read block of data. */
    for (i=0; i<1024; i+=READ_NBYTES)
        read_page(a, addr+i, i + (unsigned char*) block);

    /* Compare. */
    for (i=0; i<nwords; i++) {
        expected = data [i];
        word = block [i];
        if (word != expected) {
            printf("\nerror at address %08X: file=%08X, mem=%08X\n",
                addr + i*4, expected, word);
            exit(1);
        }
    }
}

/*
 * Flash write, 1-kbyte blocks.
 */
static void stk_program_block(adapter_t *adapter,
    unsigned addr, unsigned *data)
{
    stk_adapter_t *a = (stk_adapter_t*) adapter;
    unsigned i;

    for (i=0; i<1024/4; ++i) {
        write_byte(a, addr + i*4,     data[i]);
        write_byte(a, addr + i*4 + 1, data[i] >> 8);
        write_byte(a, addr + i*4 + 2, data[i] >> 16);
        write_byte(a, addr + i*4 + 3, data[i] >> 24);
    }
    flush_write_buffer(a);
}

adapter_t *adapter_open_stk500v2(const char *port, int baud_rate)
{
    stk_adapter_t *a;

    a = calloc(1, sizeof(*a));
    if (! a) {
        fprintf(stderr, "Out of memory\n");
        return 0;
    }
    a->baud = baud_rate;
    a->first_time = 1;
    a->timeout_msec = 1000;

    /* Open serial port */
    if (serial_open(port, baud_rate) < 0) {
        /* failed to open serial port */
        free(a);
        return 0;
    }
    usleep(200000);

    /* Detect the device. */
    int retry_count;
    unsigned char response [12];

    /* Synchronize */
    retry_count = 0;
    int outer_retry = 0;
    for (;;) {
        /* Send CMD_SIGN_ON. */
        if (send_receive(a, (unsigned char*)"\1", 1, response, 11) &&
            ((memcmp(response, "\1\0\10STK500_2", 11) == 0)
            ||
            (memcmp(response, "\1\0\10AVRISP_2", 11) == 0))) {
            if (debug_level > 1)
                printf("stk-probe: OK\n");
            break;
        }
        ++retry_count;
        if (debug_level > 1) {
            printf("stk-probe: retry %d: "
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
            serial_open(port, baud_rate);
            outer_retry++;
        }
        if (outer_retry >= 2) {
            free(a);
            serial_close();
            return 0;
        }
    }

    switch_baud(a);

    prog_enable(a);
    a->last_load_addr = -1;

    /* Identify device. */
    printf("      Adapter: STK500v2 Bootloader\n");

    a->adapter.user_start = 0x1d000000;
    a->adapter.user_nbytes = 2048 * 1024;
    a->adapter.boot_nbytes = 80 * 1024;
    a->adapter.block_override = 1024;
    a->adapter.flags = (AD_PROBE | AD_ERASE | AD_READ | AD_WRITE);

    printf(" Program area: %08x-%08x\n", a->adapter.user_start,
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
