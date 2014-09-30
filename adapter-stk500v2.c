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

/* STK status constants */
#define STATUS_CMD_OK           0x00    /* Success */

typedef struct {
    /* Common part */
    adapter_t adapter;

//    unsigned char reply [64];
//    int reply_len;

    char            *name;
    unsigned char   sequence_number;
    unsigned char   page_addr_fetched;
    unsigned short  page_size;
    unsigned short  page_delay;
    u_int32_t       page_addr;
    u_int32_t       last_load_addr;
    unsigned char   page [256];

#if defined(__WIN32__) || defined(WIN32)
    void            *fd;
    DCB             saved_mode;
#else
    int             fd;
    struct termios  saved_mode;
#endif

} stk_adapter_t;

/*
 * Receive data from device.
 */
static int stk_receive (stk_adapter_t *a, unsigned char *data, int len)
{
#if defined(__WIN32__) || defined(WIN32)
    DWORD got;

    if (! ReadFile (a->fd, data, len, &got, 0)) {
        fprintf (stderr, "stk-send: read error\n");
        exit (-1);
    }
#else
    struct timeval timeout, to2;
    long got;
    fd_set rfds;

    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    to2 = timeout;
again:
    FD_ZERO (&rfds);
    FD_SET (a->fd, &rfds);

    got = select (a->fd + 1, &rfds, 0, 0, &to2);
    if (got < 0) {
        if (errno == EINTR || errno == EAGAIN) {
            if (debug_level > 1)
                printf ("stk-send: programmer is not responding\n");
            goto again;
        }
        fprintf (stderr, "stk-send: select error: %s\n", strerror (errno));
        exit (1);
    }
#endif
    if (got == 0) {
        if (debug_level > 1)
            printf ("stk-send: programmer is not responding\n");
        return 0;
    }

#if ! defined(__WIN32__) && !defined(WIN32)
    got = read (a->fd, data, (len > 1024) ? 1024 : len);
    if (got < 0) {
        fprintf (stderr, "stk-send: read error\n");
        exit (-1);
    }
#endif
    return got;
}


/*
 * Send the command sequence and get back a response.
 */
static int stk_send (stk_adapter_t *a, unsigned char *cmd, int cmdlen,
    unsigned char *response, int reply_len)
{
    unsigned char *p, sum, hdr [5];
    int len, i, got, rlen;

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
#if defined(__WIN32__) || defined(WIN32)
    {
    DWORD written;

    if (! WriteFile (a->fd, hdr, 5, &written, 0) ||
        ! WriteFile (a->fd, cmd, cmdlen, &written, 0) ||
        ! WriteFile (a->fd, &sum, 1, &written, 0)) {
        fprintf (stderr, "stk-send: write error\n");
        exit (-1);
    }
    }
#else
    if (write (a->fd, hdr, 5) < 0 ||
        write (a->fd, cmd, cmdlen) < 0 ||
        write (a->fd, &sum, 1) < 0) {
        fprintf (stderr, "stk-send: write error\n");
        exit (-1);
    }
#endif
    /*
     * Get header.
     */
    p = hdr;
    len = 0;
    while (len < 5) {
        got = stk_receive (a, p, 5 - len);
        if (! got)
            return 0;

        p += got;
        len += got;
    }
    if (hdr[0] != MESSAGE_START || hdr[1] != a->sequence_number ||
        hdr[4] != TOKEN) {
        printf ("got invalid header: %x-%x-%x-%x-%x\n",
            hdr[0], hdr[1], hdr[2], hdr[3], hdr[4]);
flush_input:
        {
            /* Skip all incoming data. */
            unsigned char buf [300];
            stk_receive (a, buf, sizeof (buf));
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
        got = stk_receive (a, p, rlen - len);
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
        got = stk_receive (a, p, 1);
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

static void stk_prog_enable (stk_adapter_t *a)
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

    if (! stk_send (a, cmd, 12, response, 2) || response[0] != cmd[0] ||
        response[1] != STATUS_CMD_OK) {
        fprintf (stderr, "Cannot enter programming mode.\n");
        exit (-1);
    }
}

static void stk_prog_disable (stk_adapter_t *a)
{
    unsigned char cmd [3] = { CMD_LEAVE_PROGMODE_ISP,
        1,      /* pre-delay in msec */
        1,      /* post-delay in msec */
    };
    unsigned char response [2];

    if (! stk_send (a, cmd, 3, response, 2) || response[0] != cmd[0] ||
        response[1] != STATUS_CMD_OK) {
        fprintf (stderr, "Cannot leave programming mode.\n");
        exit (-1);
    }
}

static void stk_close (adapter_t *adapter, int power_on)
{
    stk_adapter_t *a = (stk_adapter_t*) adapter;

    stk_prog_disable (a);

    /* restore and close serial port */
#if defined(__WIN32__) || defined(WIN32)
    SetCommState (a->fd, &a->saved_mode);
    CloseHandle (a->fd);
#else
    tcsetattr (a->fd, TCSANOW, &a->saved_mode);
    close (a->fd);
#endif
    free (a);
}

/*
 * Return the Device Identification code
 */
static unsigned stk_get_idcode (adapter_t *adapter)
{
    //TODO
    return 0xDEAFB00B;
}

/*
 * Read a configuration word from memory.
 */
static unsigned stk_read_word (adapter_t *adapter, unsigned addr)
{
    //TODO
    return 0;
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
 * Verify a block of memory.
 */
static void stk_verify_data (adapter_t *adapter,
    unsigned addr, unsigned nwords, unsigned *data)
{
    //stk_adapter_t *a = (stk_adapter_t*) adapter;

    //TODO
}

/*
 * Flash write, 1-kbyte blocks.
 */
static void stk_program_block (adapter_t *adapter,
    unsigned addr, unsigned *data)
{
    //stk_adapter_t *a = (stk_adapter_t*) adapter;

    //TODO
}

/*
 * Open the device.
 */
static int open_port (stk_adapter_t *a, const char *devname)
{
#if defined(__WIN32__) || defined(WIN32)
    DCB new_mode;
    COMMTIMEOUTS ctmo;
#else
    struct termios new_mode;
#endif

#if defined(__WIN32__) || defined(WIN32)
    /* Open port */
    a->fd = CreateFile (devname, GENERIC_READ | GENERIC_WRITE,
        0, 0, OPEN_EXISTING, 0, 0);
    if (a->fd == INVALID_HANDLE_VALUE) {
        fprintf (stderr, "%s: Cannot open\n", devname);
        return -1;
    }

    /* Set serial attributes */
    memset (&a->saved_mode, 0, sizeof(a->saved_mode));
    if (! GetCommState (a->fd, &a->saved_mode)) {
        fprintf (stderr, "%s: Cannot get state\n", devname);
        return -1;
    }

    new_mode = a->saved_mode;

    new_mode.BaudRate = CBR_115200;
    new_mode.ByteSize = 8;
    new_mode.StopBits = ONESTOPBIT;
    new_mode.Parity = 0;
    new_mode.fParity = FALSE;
    new_mode.fOutX = FALSE;
    new_mode.fInX = FALSE;
    new_mode.fOutxCtsFlow = FALSE;
    new_mode.fOutxDsrFlow = FALSE;
    new_mode.fRtsControl = RTS_CONTROL_ENABLE;
    new_mode.fNull = FALSE;
    new_mode.fAbortOnError = FALSE;
    new_mode.fBinary = TRUE;
    if (! SetCommState (a->fd, &new_mode)) {
        fprintf (stderr, "%s: Cannot set state\n", devname);
        return -1;
    }

    memset (&ctmo, 0, sizeof(ctmo));
    ctmo.ReadIntervalTimeout = 0;
    ctmo.ReadTotalTimeoutMultiplier = 0;
    ctmo.ReadTotalTimeoutConstant = 5000;
    if (! SetCommTimeouts (a->fd, &ctmo)) {
        fprintf (stderr, "%s: Cannot set timeouts\n", devname);
        return -1;
    }
#else
    /* Open port */
    a->fd = open (devname, O_RDWR | O_NONBLOCK);
    if (a->fd < 0) {
        perror (devname);
        return -1;
    }

    /* Set serial attributes */
    memset (&a->saved_mode, 0, sizeof(a->saved_mode));
    tcgetattr (a->fd, &a->saved_mode);

    new_mode = a->saved_mode;

    cfmakeraw (&new_mode);
    cfsetispeed (&new_mode, 115200);
    cfsetospeed (&new_mode, 115200);
    new_mode.c_iflag |= IGNBRK;
    tcsetattr (a->fd, TCSANOW, &new_mode);
#endif
    return 0;
}

adapter_t *adapter_open_stk500v2 (const char *port)
{
    stk_adapter_t *a;

    a = calloc (1, sizeof (*a));
    if (! a) {
        fprintf (stderr, "Out of memory\n");
        return 0;
    }

    /* Open serial port */
    if (open_port(a, port) < 0) {
        /* failed to open serial port */
        free (a);
        return 0;
    }

    /* Detect the device. */
    int retry_count;
    unsigned char response [12];

    /* Synchronize */
    retry_count = 0;
    for (;;) {
        /* Send CMD_SIGN_ON. */
        if (stk_send (a, (unsigned char*)"\1", 1, response, 11) &&
            memcmp (response, "\1\0\10STK500_2", 11) == 0) {
            if (debug_level > 1)
                printf ("stk-probe: OK\n");
            break;
        }
        ++retry_count;
        if (debug_level > 1)
            printf ("stk-probe: error %d\n", retry_count);
        if (retry_count >= 3) {
            /* Bad reply or no device connected */
            return 0;
        }
    }

    stk_prog_enable (a);
    a->last_load_addr = -1;

    /* Identify device. */
    a->page_size = 256;
    //a->page_delay = 5;

    printf ("      Adapter: STK500v2 Bootloader\n");

    a->adapter.user_start = 0x1d000000;
    a->adapter.user_nbytes = 512 * 1024;
    printf (" Program area: %08x-%08x\n", a->adapter.user_start,
        a->adapter.user_start + a->adapter.user_nbytes - 1);

    /* User functions. */
    a->adapter.close = stk_close;
    a->adapter.get_idcode = stk_get_idcode;
    a->adapter.read_word = stk_read_word;
    a->adapter.verify_data = stk_verify_data;
    a->adapter.program_block = stk_program_block;
    a->adapter.program_word = stk_program_word;
    return &a->adapter;
}

#if 0
static void load_address (stk_adapter_t *a, u_int32_t addr)
{
    unsigned char cmd [5] = { CMD_LOAD_ADDRESS,
        addr >> 24, addr >> 16, addr >> 8, addr };
    unsigned char response [2];

    if (a->last_load_addr == addr)
        return;

    if (debug > 1)
        printf ("Load address: %#x\n", addr);

    /* Extended address flag. */
    if (a->flash_size > 0x10000)
        cmd[1] |= 0x80;

    if (! stk_send (a, cmd, 5, response, 2) || response[0] != cmd[0] ||
        response[1] != STATUS_CMD_OK) {
        fprintf (stderr, "Load address failed.\n");
        exit (-1);
    }
    a->last_load_addr = addr;
}

static void flush_write_buffer (stk_adapter_t *a)
{
    unsigned char cmd [10+256] = { CMD_PROGRAM_FLASH_ISP,
        a->page_size >> 8, a->page_size,
        0xA1,                   /* mode */
        a->page_delay * 3/2,  /* delay */
        0x40,                   /* Load Page command */
        0x4c,                   /* Write Program Memory Page command */
        0x20,                   /* Read Program Memory command */
        0xFF,                   /* poll value 1 */
        0 };                    /* poll value 2 (for EEPROM only) */
    unsigned char response [2];
    int page_bytes = a->page_size;

    if (! a->page_addr_fetched)
        return;

    load_address (a, a->page_addr >> 1);

    if (debug > 1)
        printf ("Programming page: %#x\n", a->page_addr);

    if (page_bytes == 0) {
        /* Word mode. */
        page_bytes = 256;
        cmd [1] = page_bytes >> 8;
        cmd [2] = page_bytes;
        cmd [3] = 4;            /* mode */
        cmd [6] = 0x20;         /* Read Program Memory command */
        cmd [8] = 0x7F;         /* poll value 1 */
    }
    memcpy (cmd+10, a->page, page_bytes);
    if (! stk_send (a, cmd, 10+page_bytes, response, 2) ||
        response[0] != cmd[0]) {
        fprintf (stderr, "Program flash failed.\n");
        exit (-1);
    }
    if (response[1] != STATUS_CMD_OK)
        printf ("Programming flash: timeout at %#x\n", a->page_addr);

    usleep (a->page_delay * 1000L);
    a->page_addr_fetched = 0;
    a->last_load_addr += page_bytes / 2;
}

/*
 * PAGE MODE PROGRAMMING:
 * Cache page address. When current address is out of the page address,
 * flush page buffer and continue programming.
 */
void stk_write_byte (stk_adapter_t *a, u_int32_t addr, unsigned char byte)
{
    int page_bytes = a->page_size;

    if (page_bytes == 0) {
        /* Word mode. */
        page_bytes = 256;
    }
    if (debug > 2)
        printf ("Loading to address: %#x (page_addr_fetched=%s)\n",
            addr, a->page_addr_fetched ? "Yes" : "No");

    if (a->page_addr / page_bytes != addr / page_bytes)
        flush_write_buffer (a);

    if (! a->page_addr_fetched) {
        a->page_addr = addr / page_bytes * page_bytes;
        a->page_addr_fetched = 1;
    }
    a->page [addr % page_bytes] = byte;
}

unsigned char stk_read_byte (stk_adapter_t *a, u_int32_t addr)
{
    unsigned char cmd [4] = { CMD_READ_FLASH_ISP, 0, 2, 0x20 };
    unsigned char response [5];

    load_address (a, addr >> 1);
    if (! stk_send (a, cmd, 4, response, 5) || response[0] != cmd[0] ||
        response[1] != STATUS_CMD_OK || response[4] != STATUS_CMD_OK) {
        fprintf (stderr, "Read byte failed.\n");
        exit (-1);
    }
    return (addr & 1) ? response[3] : response[2];
}

void stk_write_block (stk_adapter_t *a, u_int32_t addr,
    unsigned char *buf, u_int32_t bytes)
{
    unsigned short i;

    for (i=0; i<bytes; ++i) {
        stk_write_byte (a, addr+i, buf[i]);
    }
    flush_write_buffer (a);
}

int stk_check_block (stk_adapter_t *a, u_int32_t addr,
    unsigned char *buf, u_int32_t bytes)
{
    unsigned short i;
    unsigned char page [256];

    stk_read_block (a, addr, page, 256);
    for (i=0; i<bytes; ++i) {
        if (page[i] != buf[i]) {
            printf ("\nerror at address %#x: file=%#x, mem=%#x\n",
                addr+i, buf[i], page[i]);
            return 0;
        }
    }
    return 1;
}

void stk_read_block (stk_adapter_t *a, u_int32_t addr,
    unsigned char *buf, u_int32_t bytes)
{
    unsigned char cmd [4] = { CMD_READ_FLASH_ISP, 1, 0, 0x20 };
    unsigned char response [3+256];

    load_address (a, addr >> 1);
again:
    if (debug > 1)
        printf ("Read page: %#x\n", addr);

    if (! stk_send (a, cmd, 4, response, 3+256) ||
        response[0] != cmd[0] ||
        response[1] != STATUS_CMD_OK ||
        response[2+256] != STATUS_CMD_OK) {
        fprintf (stderr, "Read page failed.\n");
        exit (-1);
    }
    memcpy (buf, response+2, 256);
    bytes -= 256;
    if (bytes > 0) {
        addr += 256;
        buf += 256;
        goto again;
    }
    a->last_load_addr += 256 / 2;
}
#endif
