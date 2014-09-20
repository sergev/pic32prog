/*
 * Interface to PIC32 Microchip AN1388 UART bootloader (new).
 *
 * Copyright (C) 2011 Serge Vakulenko
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

#include <termios.h>
#include <sys/time.h>
#include <sys/types.h>

#include "adapter.h"
#include "pic32.h"

#define FRAME_SOH           0x01
#define FRAME_EOT           0x04
#define FRAME_DLE           0x10

#define CMD_READ_VERSION    0x01
#define CMD_ERASE_FLASH     0x02
#define CMD_PROGRAM_FLASH   0x03
#define CMD_READ_CRC        0x04
#define CMD_JUMP_APP        0x05

struct termios oldtio;

typedef struct {
    /* Common part */
    adapter_t adapter;

    unsigned char reply [64];
    int reply_len;

    int port_fd;

} an1388_adapter_t;

/*
 * Identifiers of USB adapter.
 */
#define MICROCHIP_VID           0x04d8
#define BOOTLOADER_PID          0x003c  /* Microchip AN1388 Bootloader */

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

static void an1388_send (int port_fd, unsigned char *buf, unsigned nbytes)
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
    write (port_fd, buf, nbytes);
}

static int an1388_recv (int port_fd, unsigned char *buf)
{
    int i, res;
    struct timeval tv;
    fd_set fdset;

    FD_ZERO (&fdset);
    FD_SET (port_fd, &fdset);

    /* set port timeout to 1 sec */
    tv.tv_usec = 0;
    tv.tv_sec  = 1;

    res = select(port_fd+1, &fdset, NULL, NULL, &tv);
    if (res <= 0) {
        /* select timed out or there is error */
        return -1;
    } else if (! FD_ISSET (port_fd, &fdset)) {
        /* port_fd is not in the set! */
        return -1;
    } else {
        res = read (port_fd, buf, 64);

        if (debug_level > 0) {
            fprintf (stderr, "---Recv");
            for (i=0; i<res; ++i) {
                if (i != 0 && (i & 15) == 0)
                    fprintf (stderr, "\n       ");
                fprintf (stderr, " %02x", buf[i]);
            }
            fprintf (stderr, "\n");
        }

        return res;
    }
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
static void an1388_command (an1388_adapter_t *a, unsigned char cmd,
    unsigned char *data, unsigned data_len)
{
    unsigned char buf [64];
    unsigned i, n, c, crc;
    int  res, esc;

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
    an1388_send (a->port_fd, buf, n);

    if (cmd == CMD_JUMP_APP) {
        /* No reply expected. */
        return;
    }

    /* serial port receives chunks of bytes  */
    c = 0;
    esc = 0;
    while(1) {
        res = an1388_recv (a->port_fd, buf);
        /* timeout */
        if (res < 0) {
            a->reply_len = 0;
            return;
        }
        for (i=0; i<res; ++i) {
            switch (buf[i]) {
            default:
                a->reply[c++] = buf[i];
                continue;
            case FRAME_DLE:
                if (esc) {
                    a->reply[c++] = buf[i];
                    esc = 0;
                } else {
                    esc = 1;
                }
                continue;
            case FRAME_SOH:
                if (esc) {
                    a->reply[c++] = buf[i];
                    esc = 0;
                } else {
                    c = 0;
                }
                continue;
            case FRAME_EOT:
                if (esc) {
                    a->reply[c++] = buf[i];
                    esc = 0;
                    continue;
                }
                a->reply_len = 0;
                if (c > 2) {
                    unsigned crc = a->reply[c-2] | (a->reply[c-1] << 8);
                    if (crc == calculate_crc(0, a->reply, c-2)) {
                        a->reply_len = c - 2;
                    }
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
}

static void an1388_close (adapter_t *adapter, int power_on)
{
    an1388_adapter_t *a = (an1388_adapter_t*) adapter;

    /* Jump to application. */
    an1388_command (a, CMD_JUMP_APP, 0, 0);

    /* restore and close serial port */
    tcsetattr (a->port_fd, TCSANOW, &oldtio);
    close (a->port_fd);

    free (a);
}

/*
 * Return the Device Identification code
 */
static unsigned an1388_get_idcode (adapter_t *adapter)
{
    return 0xDEAFB00B;
}

/*
 * Read a configuration word from memory.
 */
static unsigned an1388_read_word (adapter_t *adapter, unsigned addr)
{
    /* Not supported by booloader. */
    return 0;
}

/*
 * Write a configuration word to flash memory.
 */
static void an1388_program_word (adapter_t *adapter,
    unsigned addr, unsigned word)
{
    /* Not supported by booloader. */
    if (debug_level > 0)
        fprintf (stderr, "uart: program word at %08x: %08x\n", addr, word);
}

/*
 * Verify a block of memory.
 */
static void an1388_verify_data (adapter_t *adapter,
    unsigned addr, unsigned nwords, unsigned *data)
{
    an1388_adapter_t *a = (an1388_adapter_t*) adapter;
    unsigned char request [8];
    unsigned data_crc, flash_crc, nbytes = nwords * 4;

    //fprintf (stderr, "uart: verify %d bytes at %08x\n", nbytes, addr);
    request[0] = addr;
    request[1] = addr >> 8;
    request[2] = addr >> 16;
    request[3] = (addr >> 24) + 0x80;
    request[4] = nbytes;
    request[5] = nbytes >> 8;
    request[6] = nbytes >> 16;
    request[7] = nbytes >> 24;
    an1388_command (a, CMD_READ_CRC, request, 8);
    if (a->reply_len != 3 || a->reply[0] != CMD_READ_CRC) {
        fprintf (stderr, "uart: cannot read crc at %08x\n", addr);
        exit (-1);
    }
    flash_crc = a->reply[1] | a->reply[2] << 8;

    data_crc = calculate_crc (0, (unsigned char*) data, nbytes);
    if (flash_crc != data_crc) {
        fprintf (stderr, "uart: checksum failed at %08x: sum=%04x, expected=%04x\n",
            addr, flash_crc, data_crc);
        //exit (-1);
    }
}

static void set_flash_address (an1388_adapter_t *a, unsigned addr)
{
    unsigned char request[7];
    unsigned sum, i;

    request[0] = 2;
    request[1] = 0;
    request[2] = 0;
    request[3] = 4;             /* Type: linear address record */
    request[4] = addr >> 24;
    request[5] = addr >> 16;

    /* Compute checksum. */
    sum = 0;
    for (i=0; i<6; i++)
        sum += request[i];
    request[6] = -sum;

    an1388_command (a, CMD_PROGRAM_FLASH, request, 7);
    if (a->reply_len != 1 || a->reply[0] != CMD_PROGRAM_FLASH) {
        fprintf (stderr, "uart: error setting flash address at %08x\n", addr);
        exit (-1);
    }
}

static void program_flash (an1388_adapter_t *a,
    unsigned addr, unsigned char *data, unsigned nbytes)
{
    unsigned char request[64];
    unsigned sum, empty, i;

    /* Skip empty blocks. */
    empty = 1;
    for (i=0; i<nbytes; i++) {
        if (data[i] != 0xff) {
            empty = 0;
            break;
        }
    }
    if (empty)
        return;
    //fprintf (stderr, "uart: program %d bytes at %08x: %02x-%02x-...-%02x\n",
    //    nbytes, addr, data[0], data[1], data[31]);

    request[0] = nbytes;
    request[1] = addr >> 8;
    request[2] = addr;
    request[3] = 0;             /* Type: data record */
    memcpy (request+4, data, nbytes);

    /* Compute checksum. */
    sum = 0;
    empty = 1;
    for (i=0; i<nbytes+4; i++) {
        sum += request[i];
    }
    request[nbytes+4] = -sum;

    an1388_command (a, CMD_PROGRAM_FLASH, request, nbytes + 5);
    if (a->reply_len != 1 || a->reply[0] != CMD_PROGRAM_FLASH) {
        fprintf (stderr, "uart: error programming flash at %08x\n", addr);
        exit (-1);
    }
}

/*
 * Flash write, 1-kbyte blocks.
 */
static void an1388_program_block (adapter_t *adapter,
    unsigned addr, unsigned *data)
{
    an1388_adapter_t *a = (an1388_adapter_t*) adapter;
    unsigned i;

    set_flash_address (a, addr);
    for (i=0; i<256; i+=8) {
        /* 8 words per cycle. */
        program_flash (a, addr, (unsigned char*) data, 32);
        data += 8;
        addr += 32;
    }
}

/*
 * Erase all flash memory.
 */
static void an1388_erase_chip (adapter_t *adapter)
{
    an1388_adapter_t *a = (an1388_adapter_t*) adapter;

    //fprintf (stderr, "uart: erase chip\n");
    an1388_command (a, CMD_ERASE_FLASH, 0, 0);
    if (a->reply_len != 1 || a->reply[0] != CMD_ERASE_FLASH) {
        fprintf (stderr, "uart: Erase failed\n");
        exit (-1);
    }
}

/*
 * Initialize adapter uart.
 * Return a pointer to a data structure, allocated dynamically.
 * When adapter not found, return 0.
 */
int open_port (const char *port, long baud)
{
    int fd;
    struct termios newtio;

    /* open in blocking mode */
    fd = open (port, O_RDWR | O_NOCTTY);
    if (fd < 0) {
        /* cannot open port */
        return -1;
    }
    /* save current port settings */
    tcgetattr (fd, &oldtio);

    /* clear config */
    memset (&newtio, 0, sizeof(newtio));

    /* 8n1, ignore parity */
    newtio.c_cflag = baud | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;
    newtio.c_lflag = 0;
    newtio.c_cc[VTIME] = 10;    /* 100 ms timeout */
    newtio.c_cc[VMIN]  = 0;

    /* clear port and update settings */
    tcflush (fd, TCIFLUSH);
    if (tcsetattr (fd, TCSANOW, &newtio) < 0) {
        /* unable to set port settingss */
        return -1;
    }

    return fd;
}

adapter_t *adapter_open_an1388_uart (const char *port)
{
    an1388_adapter_t *a;
    int fd;
    long bdrate = B115200;

    /* open serial port */
    fd = open_port(port, bdrate);
    if (fd < 0) {
        /* failed to open serial port */
        return 0;
    }

    a = calloc (1, sizeof (*a));
    if (! a) {
        fprintf (stderr, "Out of memory\n");
        return 0;
    }
    a->port_fd = fd;

    /* Read version of adapter. */
    an1388_command (a, CMD_READ_VERSION, 0, 0);
    if (a->reply_len == 0) {
        /* bad reply or no device connected */
        return 0;
    }
    printf ("      Adapter: AN1388 UART Bootloader Version %d.%d\n",
        a->reply[1], a->reply[2]);

    a->adapter.user_start = 0x1d000000;
    a->adapter.user_nbytes = 512 * 1024;
    printf (" Program area: %08x-%08x\n", a->adapter.user_start,
        a->adapter.user_start + a->adapter.user_nbytes - 1);

    /* User functions. */
    a->adapter.close = an1388_close;
    a->adapter.get_idcode = an1388_get_idcode;
    a->adapter.read_word = an1388_read_word;
    a->adapter.verify_data = an1388_verify_data;
    a->adapter.erase_chip = an1388_erase_chip;
    a->adapter.program_block = an1388_program_block;
    a->adapter.program_word = an1388_program_word;
    return &a->adapter;
}
