/*
 * Interface to PIC32 JTAG port using FT2232-based USB adapter.
 * Supported hardware:
 *  - Olimex ARM-USB-Tiny adapter
 *  - Olimex ARM-USB-Tiny-H adapter
 *  - Olimex ARM-USB-OCD-H adapter
 *  - Olimex MIPS-USB-OCD-H adapter
 *  - Bus Blaster v2 from Dangerous Prototypes
 *  - TinCanTools Flyswatter adapter
 *
 * Copyright (C) 2011-2013 Serge Vakulenko
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
#ifdef __FreeBSD__
#include <libusb.h>
#else
#include <libusb-1.0/libusb.h>
#endif

#include "adapter.h"
#include "pic32.h"

typedef struct {
    uint16_t vid;
    uint16_t pid;
    const char *name;
    uint8_t mhz;
    uint16_t dir_control;
    uint16_t trst_control;
    uint8_t trst_inverted;
    uint16_t sysrst_control;
    uint8_t sysrst_inverted;
    uint16_t led_control;
    uint8_t led_inverted;
    const char *product;
    uint16_t extra_output;
} device_t;

typedef struct {
    /* Common part */
    adapter_t adapter;
    const char *name;

    /* Device handle for libusb. */
    libusb_device_handle *usbdev;
    libusb_context *context;

    /* Transmit buffer for MPSSE packet. */
    unsigned char output [256*16];
    int bytes_to_write;

    /* Receive buffer. */
    unsigned char input [64];
    int bytes_to_read;
    int bytes_per_word;
    unsigned long long fix_high_bit;
    unsigned long long high_byte_mask;
    unsigned long long high_bit_mask;
    unsigned high_byte_bits;

    /* Mapping of /TRST, /SYSRST and LED control signals. */
    unsigned trst_control, trst_inverted;
    unsigned sysrst_control, sysrst_inverted;
    unsigned led_control, led_inverted;
    unsigned dir_control;
    unsigned extra_output;

    unsigned mhz;
    unsigned use_executive;
    unsigned serial_execution_mode;
} mpsse_adapter_t;

/*
 * Identifiers of USB adapter.
 */
#define OLIMEX_VID              0x15ba
#define OLIMEX_ARM_USB_TINY     0x0004  /* ARM-USB-Tiny */
#define OLIMEX_ARM_USB_TINY_H   0x002a  /* ARM-USB-Tiny-H */
#define OLIMEX_ARM_USB_OCD_H    0x002b  /* ARM-USB-OCD-H */
#define OLIMEX_MIPS_USB_OCD_H   0x0036  /* MIPS-USB-OCD-H */

#define FTDI_DEFAULT_VID        0x0403  /* Neofoxx JTAG/SWD debug probe, Bus Blaster v2, Flyswatter, ...*/
#define FTDI_DEFAULT_PID        0x6010

/*
 * USB endpoints.
 */
#define IN_EP                   0x02
#define OUT_EP                  0x81

/* Requests */
#define SIO_RESET               0 /* Reset the port */
#define SIO_MODEM_CTRL          1 /* Set the modem control register */
#define SIO_SET_FLOW_CTRL       2 /* Set flow control register */
#define SIO_SET_BAUD_RATE       3 /* Set baud rate */
#define SIO_SET_DATA            4 /* Set the data characteristics of the port */
#define SIO_POLL_MODEM_STATUS   5
#define SIO_SET_EVENT_CHAR      6
#define SIO_SET_ERROR_CHAR      7
#define SIO_SET_LATENCY_TIMER   9
#define SIO_GET_LATENCY_TIMER   10
#define SIO_SET_BITMODE         11
#define SIO_READ_PINS           12
#define SIO_READ_EEPROM         0x90
#define SIO_WRITE_EEPROM        0x91
#define SIO_ERASE_EEPROM        0x92

/* MPSSE commands. */
#define CLKWNEG                 0x01
#define BITMODE                 0x02
#define CLKRNEG                 0x04
#define LSB                     0x08
#define WTDI                    0x10
#define RTDO                    0x20
#define WTMS                    0x40

/* TMS header and footer defines */
#define TMS_HEADER_COMMAND_NBITS        4
#define TMS_HEADER_COMMAND_VAL          0b0011
#define TMS_HEADER_XFERDATA_NBITS       3
#define TMS_HEADER_XFERDATA_VAL         0b001
#define TMS_HEADER_XFERDATAFAST_NBITS   3
#define TMS_HEADER_XFERDATAFAST_VAL     0b001
#define TMS_HEADER_RESET_TAP_NBITS      6
#define TMS_HEADER_RESET_TAP_VAL        0b011111

#define TMS_FOOTER_COMMAND_NBITS        3
#define TMS_FOOTER_COMMAND_VAL          0b001
#define TMS_FOOTER_XFERDATA_NBITS       3
#define TMS_FOOTER_XFERDATA_VAL         0b001
#define TMS_FOOTER_XFERDATAFAST_NBITS   2
#define TMS_FOOTER_XFERDATAFAST_VAL     0b01

static const device_t devlist[] = {
    { OLIMEX_VID,           OLIMEX_ARM_USB_TINY,    "Olimex ARM-USB-Tiny",               6,  0x0f10, 0x0100, 1,  0x0200,  0,   0x0800,  0, NULL, 0x0000},
    { OLIMEX_VID,           OLIMEX_ARM_USB_TINY_H,  "Olimex ARM-USB-Tiny-H",            30,  0x0f10, 0x0100, 1,  0x0200,  0,   0x0800,  0, NULL, 0x0000},
    { OLIMEX_VID,           OLIMEX_ARM_USB_OCD_H,   "Olimex ARM-USB-OCD-H",             30,  0x0f10, 0x0100, 1,  0x0200,  0,   0x0800,  0, NULL, 0x0000},
    { OLIMEX_VID,           OLIMEX_MIPS_USB_OCD_H,  "Olimex MIPS-USB-OCD-H",            30,  0x0f10, 0x0100, 1,  0x0200,  1,   0x0800,  0, NULL, 0x0000},
    { FTDI_DEFAULT_VID,     FTDI_DEFAULT_PID,       "TinCanTools Flyswatter",            6,  0x0cf0, 0x0010, 1,  0x0020,  1,   0x0c00,  1, "Flyswatter", 0x0000},
    { FTDI_DEFAULT_VID,     FTDI_DEFAULT_PID,       "Neofoxx JTAG/SWD adapter",         30,  0xff3b, 0x0100, 1,  0x0200,  1,   0x8000,  1, "Neofoxx JTAG/SWD adapter", 0x0020},
    { FTDI_DEFAULT_VID,     FTDI_DEFAULT_PID,       "Dangerous Prototypes Bus Blaster", 30,  0x0f10, 0x0100, 1,  0x0200,  1,   0x0000,  0, NULL, 0x0000},
    { 0 }
};

/*
 * Calculate checksum.
 */
static unsigned calculate_crc(unsigned crc, unsigned char *data, unsigned nbytes)
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

/*
 * Send a packet to USB device.
 */
static void bulk_write(mpsse_adapter_t *a, unsigned char *output, int nbytes)
{
    int bytes_written;

    if (debug_level > 1) {
        int i;
        fprintf(stderr, "usb bulk write %d bytes:", nbytes);
        for (i=0; i<nbytes; i++)
            fprintf(stderr, "%c%02x", i ? '-' : ' ', output[i]);
        fprintf(stderr, "\n");
    }

    int ret = libusb_bulk_transfer(a->usbdev, IN_EP, (unsigned char*) output,
        nbytes, &bytes_written, 1000);

    if (ret != 0) {
        fprintf(stderr, "usb bulk write failed: %d: %s\n",
            ret, libusb_strerror(ret));
        exit(-1);
    }
    if (bytes_written != nbytes)
        fprintf(stderr, "usb bulk written %d bytes of %d",
            bytes_written, nbytes);
}

/*
 * If there are any data in transmit buffer -
 * send them to device.
 */
static void mpsse_flush_output(mpsse_adapter_t *a)
{
    int bytes_read, n;
    unsigned char reply [64];

    if (a->bytes_to_write <= 0)
        return;

    bulk_write(a, a->output, a->bytes_to_write);
    a->bytes_to_write = 0;
    if (a->bytes_to_read <= 0)
        return;

    /* Get reply. */
    bytes_read = 0;
    while (bytes_read < a->bytes_to_read) {
        int ret = libusb_bulk_transfer(a->usbdev, OUT_EP, (unsigned char*) reply,
            a->bytes_to_read - bytes_read + 2, &n, 2000);
        if (ret != 0) {
            fprintf(stderr, "usb bulk read failed\n");
            exit(-1);
        }
        if (debug_level > 1) {
            if (n != a->bytes_to_read + 2)
                fprintf(stderr, "usb bulk read %d bytes of %d\n",
                    n, a->bytes_to_read - bytes_read + 2);
            else {
                int i;
                fprintf(stderr, "usb bulk read %d bytes:", n);
                for (i=0; i<n; i++)
                    fprintf(stderr, "%c%02x", i ? '-' : ' ', reply[i]);
                fprintf(stderr, "\n");
            }
        }
        if (n > 2) {
            /* Copy data. */
            memcpy(a->input + bytes_read, reply + 2, n - 2);
            bytes_read += n - 2;
        }
    }
    if (debug_level > 1) {
        int i;
        fprintf(stderr, "mpsse_flush_output received %d bytes:", a->bytes_to_read);
        for (i=0; i<a->bytes_to_read; i++)
            fprintf(stderr, "%c%02x", i ? '-' : ' ', a->input[i]);
        fprintf(stderr, "\n");
    }
    a->bytes_to_read = 0;
}

static void mpsse_send(mpsse_adapter_t *a,
    unsigned tms_prolog_nbits, unsigned tms_prolog,
    unsigned tdi_nbits, unsigned long long tdi,
    unsigned tms_epilog_nbits, unsigned tms_epilog, int read_flag)
{

    /* Check that we have enough space in output buffer.
     * Max size of one packet is 23 bytes (6+8+3+3+3). */
    if (a->bytes_to_write > sizeof(a->output) - 23)
        mpsse_flush_output(a);

    /* Prepare a packet of MPSSE commands. */
    if (tms_prolog_nbits > 0) {
        /* Prologue TMS, from 1 to 14 bits.
         * 4b - Clock Data to TMS Pin (no Read) */
        a->output [a->bytes_to_write++] = WTMS + BITMODE + CLKWNEG + LSB;
        if (tms_prolog_nbits < 8) {
            a->output [a->bytes_to_write++] = tms_prolog_nbits - 1;
            a->output [a->bytes_to_write++] = tms_prolog;
        } else {
            a->output [a->bytes_to_write++] = 7 - 1;
            a->output [a->bytes_to_write++] = tms_prolog & 0x7f;
            a->output [a->bytes_to_write++] = WTMS + BITMODE + CLKWNEG + LSB;
            a->output [a->bytes_to_write++] = tms_prolog_nbits - 7 - 1;
            a->output [a->bytes_to_write++] = tms_prolog >> 7;
        }
    }
    if (tdi_nbits > 0) {
        /* Data, from 1 to 64 bits. */
        if (tms_epilog_nbits > 0) {
            /* Last bit should be accompanied with signal TMS=1. */
            tdi_nbits--;
        }
        unsigned nbytes = tdi_nbits / 8;
        unsigned last_byte_bits = tdi_nbits & 7;
        if (read_flag) {
            a->high_byte_bits = last_byte_bits;
            a->fix_high_bit = 0;
            a->high_byte_mask = 0;
            a->bytes_per_word = nbytes;
            if (a->high_byte_bits > 0)
                a->bytes_per_word++;
            a->bytes_to_read += a->bytes_per_word;
        }
        if (nbytes > 0) {
            /* Whole bytes.
             * 39 - Clock Data Bytes In and Out LSB First
             * 19 - Clock Data Bytes Out LSB First (no Read) */
            a->output [a->bytes_to_write++] = read_flag ?
                (WTDI + RTDO + CLKWNEG + LSB) :
                (WTDI + CLKWNEG + LSB);
            a->output [a->bytes_to_write++] = nbytes - 1;
            a->output [a->bytes_to_write++] = (nbytes - 1) >> 8;
            while (nbytes-- > 0) {
                a->output [a->bytes_to_write++] = tdi;
                tdi >>= 8;
            }
        }
        if (last_byte_bits) {
            /* Last partial byte.
             * 3b - Clock Data Bits In and Out LSB First
             * 1b - Clock Data Bits Out LSB First (no Read) */
            a->output [a->bytes_to_write++] = read_flag ?
                (WTDI + RTDO + BITMODE + CLKWNEG + LSB) :
                (WTDI + BITMODE + CLKWNEG + LSB);
            a->output [a->bytes_to_write++] = last_byte_bits - 1;
            a->output [a->bytes_to_write++] = tdi;
            tdi >>= last_byte_bits;
            a->high_byte_mask = 0xffULL << (a->bytes_per_word - 1) * 8;
        }
        if (tms_epilog_nbits > 0) {
            /* Last bit (actually two bits).
             * 6b - Clock Data to TMS Pin with Read
             * 4b - Clock Data to TMS Pin (no Read) */
            tdi_nbits++;
            a->output [a->bytes_to_write++] = read_flag ?
                (WTMS + RTDO + BITMODE + CLKWNEG + LSB) :
                (WTMS + BITMODE + CLKWNEG + LSB);
            a->output [a->bytes_to_write++] = 1;
            a->output [a->bytes_to_write++] = tdi << 7 | 1 | tms_epilog << 1;
            tms_epilog_nbits--;
            tms_epilog >>= 1;
            if (read_flag) {
                /* Last bit wil come in next byte.
                 * Compute a mask for correction. */
                a->fix_high_bit = 0x40ULL << (a->bytes_per_word * 8);
                a->bytes_per_word++;
                a->bytes_to_read++;
            }
        }
        if (read_flag)
            a->high_bit_mask = 1ULL << (tdi_nbits - 1);
    }
    if (tms_epilog_nbits > 0) {
        /* Epiloque TMS, from 1 to 7 bits.
         * 4b - Clock Data to TMS Pin (no Read) */
        a->output [a->bytes_to_write++] = WTMS + BITMODE + CLKWNEG + LSB;
        a->output [a->bytes_to_write++] = tms_epilog_nbits - 1;
        a->output [a->bytes_to_write++] = tms_epilog;
    }
}

static unsigned long long mpsse_fix_data(mpsse_adapter_t *a, unsigned long long word)
{
    unsigned long long fix_high_bit = word & a->fix_high_bit;
    //if (debug) fprintf(stderr, "fix (%08llx) high_bit=%08llx\n", word, a->fix_high_bit);

    if (a->high_byte_bits) {
        /* Fix a high byte of received data. */
        unsigned long long high_byte = a->high_byte_mask &
            ((word & a->high_byte_mask) >> (8 - a->high_byte_bits));
        word = (word & ~a->high_byte_mask) | high_byte;
        //if (debug) fprintf(stderr, "Corrected byte %08llx -> %08llx\n", a->high_byte_mask, high_byte);
    }
    word &= a->high_bit_mask - 1;
    if (fix_high_bit) {
        /* Fix a high bit of received data. */
        word |= a->high_bit_mask;
        //if (debug) fprintf(stderr, "Corrected bit %08llx -> %08llx\n", a->high_bit_mask, word >> 9);
    }
    return word;
}

static unsigned long long mpsse_recv(mpsse_adapter_t *a)
{
    unsigned long long word;

    /* Send a packet. */
    mpsse_flush_output(a);

    /* Process a reply: one 64-bit word. */
    memcpy(&word, a->input, sizeof(word));
    return mpsse_fix_data(a, word);
}

static void mpsse_reset(mpsse_adapter_t *a, int trst, int sysrst, int led)
{
    unsigned char buf [3];
    unsigned output    = 0x0008 | a->extra_output;  /* TCK idle high, OE pins etc. */
    unsigned direction = 0x000b | a->dir_control;

    if (trst)
        output |= a->trst_control;
    if (a->trst_inverted)
        output ^= a->trst_control;

    if (sysrst)
        output |= a->sysrst_control;
    if (a->sysrst_inverted)
        output ^= a->sysrst_control;

    if (led)
        output |= a->led_control;
    if (a->led_inverted)
        output ^= a->led_control;

    /* command "set data bits low byte" */
    buf [0] = 0x80;
    buf [1] = output;
    buf [2] = direction;
    bulk_write(a, buf, 3);

    /* command "set data bits high byte" */
    buf [0] = 0x82;
    buf [1] = output >> 8;
    buf [2] = direction >> 8;
    bulk_write(a, buf, 3);

    if (debug_level)
        fprintf(stderr, "mpsse_reset(trst=%d, sysrst=%d) output=%04x, direction: %04x\n",
            trst, sysrst, output, direction);
}

static void mpsse_speed(mpsse_adapter_t *a, int khz)
{
    unsigned char output [3];
    int divisor = (a->mhz * 2000 / khz + 1) / 2 - 1;

    if (divisor < 0)
        divisor = 0;
    if (debug_level)
        fprintf(stderr, "%s: divisor: %u\n", a->name, divisor);

    if (a->mhz > 6) {
        /* Use 60MHz master clock (disable divide by 5). */
        output [0] = 0x8A;

        /* Turn off adaptive clocking. */
        output [1] = 0x97;

        /* Disable three-phase clocking. */
        output [2] = 0x8D;
        bulk_write(a, output, 3);
    }

    /* Command "set TCK divisor". */
    output [0] = 0x86;
    output [1] = divisor;
    output [2] = divisor >> 8;
    bulk_write(a, output, 3);

    if (debug_level) {
        khz = (a->mhz * 2000 / (divisor + 1) + 1) / 2;
        fprintf(stderr, "%s: clock rate %.1f MHz\n", a->name, khz / 1000.0);
    }
}

static void mpsse_close(adapter_t *adapter, int power_on)
{
    mpsse_adapter_t *a = (mpsse_adapter_t*) adapter;

    /* Clear EJTAGBOOT mode. */
    /* Send command. */
    mpsse_send(a, TMS_HEADER_COMMAND_NBITS, TMS_HEADER_COMMAND_VAL,
                    MTAP_COMMAND_NBITS, TAP_SW_ETAP,
                    TMS_FOOTER_COMMAND_NBITS, TMS_FOOTER_COMMAND_VAL,
                    0);
    /* TMS 1-1-1-1-1-0 */
    mpsse_send(a, TMS_HEADER_RESET_TAP_NBITS, TMS_HEADER_RESET_TAP_VAL,
                0, 0, 0, 0, 0);
    mpsse_flush_output(a);

    /* Toggle /SYSRST. */
    mpsse_reset(a, 0, 1, 1);
    mdelay(100);    /* Hold in reset for a bit, so it auto-runs afterwards */
    mpsse_reset(a, 0, 0, 0);

    libusb_release_interface(a->usbdev, 0);
    libusb_close(a->usbdev);
    free(a);
}

/*
 * Read the Device Identification code
 */
static unsigned mpsse_get_idcode(adapter_t *adapter)
{
    mpsse_adapter_t *a = (mpsse_adapter_t*) adapter;
    unsigned idcode;

    /* Reset the JTAG TAP controller: TMS 1-1-1-1-1-0.
     * After reset, the IDCODE register is always selected.
     * Read out 32 bits of data. */
    mpsse_send(a, TMS_HEADER_RESET_TAP_NBITS + TMS_HEADER_XFERDATA_NBITS,
                    TMS_HEADER_RESET_TAP_VAL + (TMS_HEADER_XFERDATA_VAL << TMS_HEADER_RESET_TAP_NBITS),
                    32, 0,
                    TMS_FOOTER_XFERDATA_NBITS, TMS_FOOTER_XFERDATA_VAL,
                    1);
    idcode = mpsse_recv(a);
    return idcode;
}

/*
 * Put device to serial execution mode.
 */
static void serial_execution(mpsse_adapter_t *a)
{
    if (a->serial_execution_mode)
        return;
    a->serial_execution_mode = 1;

    /* Enter serial execution. */
    if (debug_level > 0)
        fprintf(stderr, "%s: enter serial execution\n", a->name);

    /* Send command. */
    mpsse_send(a, TMS_HEADER_COMMAND_NBITS, TMS_HEADER_COMMAND_VAL,
                    MTAP_COMMAND_NBITS, TAP_SW_ETAP,
                    TMS_FOOTER_COMMAND_NBITS, TMS_FOOTER_COMMAND_VAL,
                    0);
    /* Send command. */
    mpsse_send(a, TMS_HEADER_COMMAND_NBITS, TMS_HEADER_COMMAND_VAL,
                    ETAP_COMMAND_NBITS, ETAP_EJTAGBOOT,
                    TMS_FOOTER_COMMAND_NBITS, TMS_FOOTER_COMMAND_VAL,
                    0);

    /* Check status. */
    /* Send command. */
    mpsse_send(a, TMS_HEADER_COMMAND_NBITS, TMS_HEADER_COMMAND_VAL,
                    MTAP_COMMAND_NBITS, TAP_SW_MTAP,
                    TMS_FOOTER_COMMAND_NBITS, TMS_FOOTER_COMMAND_VAL,
                    0);
    /* Send command. */
    mpsse_send(a, TMS_HEADER_COMMAND_NBITS, TMS_HEADER_COMMAND_VAL,
                    MTAP_COMMAND_NBITS, MTAP_COMMAND,
                    TMS_FOOTER_COMMAND_NBITS, TMS_FOOTER_COMMAND_VAL,
                    0);
    /* Xfer data. */
    mpsse_send(a, TMS_HEADER_XFERDATA_NBITS, TMS_HEADER_XFERDATA_VAL,
                    MTAP_COMMAND_DR_NBITS, MCHP_DEASSERT_RST, 
                    TMS_FOOTER_XFERDATA_NBITS, TMS_FOOTER_XFERDATA_VAL,
                    0); 
    /* Xfer data. */
    mpsse_send(a, TMS_HEADER_XFERDATA_NBITS, TMS_HEADER_XFERDATA_VAL,
                    MTAP_COMMAND_DR_NBITS, MCHP_FLASH_ENABLE, 
                    TMS_FOOTER_XFERDATA_NBITS, TMS_FOOTER_XFERDATA_VAL,
                    0);  
    /* Xfer data. */
    mpsse_send(a, TMS_HEADER_XFERDATA_NBITS, TMS_HEADER_XFERDATA_VAL,
                    MTAP_COMMAND_DR_NBITS, MCHP_STATUS, 
                    TMS_FOOTER_XFERDATA_NBITS, TMS_FOOTER_XFERDATA_VAL,
                    1);
    unsigned status = mpsse_recv(a);
    if (debug_level > 0)
        fprintf(stderr, "%s: status %04x\n", a->name, status);
    if ((status & ~MCHP_STATUS_DEVRST) !=
        (MCHP_STATUS_CPS | MCHP_STATUS_CFGRDY | MCHP_STATUS_FAEN)) {
        fprintf(stderr, "%s: invalid status = %04x (reset)\n", a->name, status);
        exit(-1);
    }

    /* Deactivate /SYSRST. */
    mpsse_reset(a, 0, 0, 1);
    mdelay(10);

    /* Check status. */
    /* Send command. */
    mpsse_send(a, TMS_HEADER_COMMAND_NBITS, TMS_HEADER_COMMAND_VAL,
                    MTAP_COMMAND_NBITS, TAP_SW_MTAP,
                    TMS_FOOTER_COMMAND_NBITS, TMS_FOOTER_COMMAND_VAL,
                    0);
    /* Send command. */
    mpsse_send(a, TMS_HEADER_COMMAND_NBITS, TMS_HEADER_COMMAND_VAL,
                    MTAP_COMMAND_NBITS, MTAP_COMMAND,
                    TMS_FOOTER_COMMAND_NBITS, TMS_FOOTER_COMMAND_VAL,
                    0);
    /* Xfer data. */
    mpsse_send(a, TMS_HEADER_XFERDATA_NBITS, TMS_HEADER_XFERDATA_VAL,
                    MTAP_COMMAND_DR_NBITS, MCHP_STATUS, 
                    TMS_FOOTER_XFERDATA_NBITS, TMS_FOOTER_XFERDATA_VAL,
                    1);  
    status = mpsse_recv(a);
    if (debug_level > 0)
        fprintf(stderr, "%s: status %04x\n", a->name, status);
    if (status != (MCHP_STATUS_CPS | MCHP_STATUS_CFGRDY |
                   MCHP_STATUS_FAEN)) {
        fprintf(stderr, "%s: invalid status = %04x (no reset)\n", a->name, status);
        exit(-1);
    }

    /* Leave it in ETAP mode. */
    /* Send command. */
    mpsse_send(a, TMS_HEADER_COMMAND_NBITS, TMS_HEADER_COMMAND_VAL,
                    MTAP_COMMAND_NBITS , TAP_SW_ETAP,
                    TMS_FOOTER_COMMAND_NBITS, TMS_FOOTER_COMMAND_VAL,
                    0);
    mpsse_flush_output(a);
}

static void xfer_fastdata(mpsse_adapter_t *a, unsigned word)
{
    mpsse_send(a, TMS_HEADER_XFERDATAFAST_NBITS, TMS_HEADER_XFERDATAFAST_VAL,
                    33, (unsigned long long) word << 1,
                    TMS_FOOTER_XFERDATAFAST_NBITS, TMS_FOOTER_XFERDATAFAST_VAL,
                    0);
}

static void xfer_instruction(mpsse_adapter_t *a, unsigned instruction)
{
    unsigned ctl;

    if (debug_level > 1)
        fprintf(stderr, "%s: xfer instruction %08x\n", a->name, instruction);

    // Select Control Register
    /* Send command. */
    mpsse_send(a, TMS_HEADER_COMMAND_NBITS, TMS_HEADER_COMMAND_VAL,
                    ETAP_COMMAND_NBITS, ETAP_CONTROL,
                    TMS_FOOTER_COMMAND_NBITS, TMS_FOOTER_COMMAND_VAL,
                    0);

    // Wait until CPU is ready
    // Check if Processor Access bit (bit 18) is set
    do {
        mpsse_send(a, TMS_HEADER_XFERDATA_NBITS, TMS_HEADER_XFERDATA_VAL,
                        32, CONTROL_PRACC |CONTROL_PROBEN |CONTROL_PROBTRAP | CONTROL_EJTAGBRK,
                        TMS_FOOTER_XFERDATA_NBITS, TMS_FOOTER_XFERDATA_VAL,
                        1);
        ctl = mpsse_recv(a);
    } while (! (ctl & CONTROL_PRACC));

    // Select Data Register
    // Send the instruction
    /* Send command. */
    mpsse_send(a, TMS_HEADER_COMMAND_NBITS, TMS_HEADER_COMMAND_VAL,
                    ETAP_COMMAND_NBITS, ETAP_DATA,
                    TMS_FOOTER_COMMAND_NBITS, TMS_FOOTER_COMMAND_VAL,
                    0);
    /* Send data. */
    mpsse_send(a, TMS_HEADER_XFERDATA_NBITS, TMS_HEADER_XFERDATA_VAL,
                    32, instruction,
                    TMS_FOOTER_XFERDATA_NBITS, TMS_FOOTER_XFERDATA_VAL,
                    0);

    // Tell CPU to execute instruction
    /* Send command. */
    mpsse_send(a, TMS_HEADER_COMMAND_NBITS, TMS_HEADER_COMMAND_VAL,
                    ETAP_COMMAND_NBITS, ETAP_CONTROL,
                    TMS_FOOTER_COMMAND_NBITS, TMS_FOOTER_COMMAND_VAL,
                    0);
    /* Send data. */
    mpsse_send(a, TMS_HEADER_XFERDATA_NBITS, TMS_HEADER_XFERDATA_VAL,
                    32, CONTROL_PROBEN | CONTROL_PROBTRAP,
                    TMS_FOOTER_XFERDATA_NBITS, TMS_FOOTER_XFERDATA_VAL,
                    0);
}

static unsigned get_pe_response(mpsse_adapter_t *a)
{
    unsigned ctl, response;

    // Select Control Register
    /* Send command. */
    mpsse_send(a, TMS_HEADER_COMMAND_NBITS, TMS_HEADER_COMMAND_VAL,
                    ETAP_COMMAND_NBITS, ETAP_CONTROL,
                    TMS_FOOTER_COMMAND_NBITS, TMS_FOOTER_COMMAND_VAL,
                    0);

    // Wait until CPU is ready
    // Check if Processor Access bit (bit 18) is set
    do {
        mpsse_send(a, TMS_HEADER_XFERDATA_NBITS, TMS_HEADER_XFERDATA_VAL,
                        32, CONTROL_PRACC |CONTROL_PROBEN |CONTROL_PROBTRAP | CONTROL_EJTAGBRK,
                        TMS_FOOTER_XFERDATA_NBITS, TMS_FOOTER_XFERDATA_VAL,
                        1);
        ctl = mpsse_recv(a);
    } while (! (ctl & CONTROL_PRACC));

    // Select Data Register
    // Send the instruction
    /* Send command. */
    mpsse_send(a, TMS_HEADER_COMMAND_NBITS, TMS_HEADER_COMMAND_VAL,
                    ETAP_COMMAND_NBITS, ETAP_DATA,
                    TMS_FOOTER_COMMAND_NBITS, TMS_FOOTER_COMMAND_VAL,
                    0);
    /* Get data. */
    mpsse_send(a, TMS_HEADER_XFERDATA_NBITS, TMS_HEADER_XFERDATA_VAL,
                    32, 0,
                    TMS_FOOTER_XFERDATA_NBITS, TMS_FOOTER_XFERDATA_VAL,
                    1);
    response = mpsse_recv(a);

    // Tell CPU to execute NOP instruction
    /* Send command. */
    mpsse_send(a, TMS_HEADER_COMMAND_NBITS, TMS_HEADER_COMMAND_VAL,
                    ETAP_COMMAND_NBITS, ETAP_CONTROL,
                    TMS_FOOTER_COMMAND_NBITS, TMS_FOOTER_COMMAND_VAL,
                    0);
    /* Send data. */
    mpsse_send(a, TMS_HEADER_XFERDATA_NBITS, TMS_HEADER_XFERDATA_VAL,
                    32, CONTROL_PROBEN | CONTROL_PROBTRAP,
                    TMS_FOOTER_XFERDATA_NBITS, TMS_FOOTER_XFERDATA_VAL,
                    0);
    if (debug_level > 1)
        fprintf(stderr, "%s: get PE response %08x\n", a->name, response);
    return response;
}

/*
 * Read a word from memory (without PE).
 */
static unsigned mpsse_read_word(adapter_t *adapter, unsigned addr)
{
    mpsse_adapter_t *a = (mpsse_adapter_t*) adapter;
    unsigned addr_lo = addr & 0xFFFF;
    unsigned addr_hi = (addr >> 16) & 0xFFFF;

    serial_execution(a);

    //fprintf(stderr, "%s: read word from %08x\n", a->name, addr);
    xfer_instruction(a, 0x3c04bf80);            // lui s3, 0xFF20
    xfer_instruction(a, 0x3c080000 | addr_hi);  // lui t0, addr_hi
    xfer_instruction(a, 0x35080000 | addr_lo);  // ori t0, addr_lo
    xfer_instruction(a, 0x8d090000);            // lw t1, 0(t0)
    xfer_instruction(a, 0xae690000);            // sw t1, 0(s3)

    /* Send command. */
    mpsse_send(a, TMS_HEADER_COMMAND_NBITS, TMS_HEADER_COMMAND_VAL,
                    ETAP_COMMAND_NBITS, ETAP_FASTDATA, 
                    TMS_FOOTER_COMMAND_NBITS, TMS_FOOTER_COMMAND_VAL,
                    0); 
    /* Get fastdata. */
    mpsse_send(a, TMS_HEADER_XFERDATAFAST_NBITS, TMS_HEADER_XFERDATAFAST_VAL,
                    33, 0,
                    TMS_FOOTER_XFERDATAFAST_NBITS, TMS_FOOTER_XFERDATAFAST_VAL,
                    1);
    unsigned word = mpsse_recv(a) >> 1;

    if (debug_level > 0)
        fprintf(stderr, "%s: read word at %08x -> %08x\n", a->name, addr, word);
    return word;
}

/*
 * Read a memory block.
 */
static void mpsse_read_data(adapter_t *adapter,
    unsigned addr, unsigned nwords, unsigned *data)
{
    mpsse_adapter_t *a = (mpsse_adapter_t*) adapter;
    unsigned words_read, i;

    //fprintf(stderr, "%s: read %d bytes from %08x\n", a->name, nwords*4, addr);
    if (! a->use_executive) {
        /* Without PE. */
        for (; nwords > 0; nwords--) {
            *data++ = mpsse_read_word(adapter, addr);
            addr += 4;
        }
        return;
    }

    /* Use PE to read memory. */
    for (words_read = 0; words_read < nwords; words_read += 32) {

        mpsse_send(a, TMS_HEADER_COMMAND_NBITS, TMS_HEADER_COMMAND_VAL,
                    ETAP_COMMAND_NBITS, ETAP_FASTDATA, 
                    TMS_FOOTER_COMMAND_NBITS, TMS_FOOTER_COMMAND_VAL,
                    0); 
        xfer_fastdata(a, PE_READ << 16 | 32);       /* Read 32 words */
        xfer_fastdata(a, addr);                     /* Address */

        unsigned response = get_pe_response(a);     /* Get response */
        if (response != PE_READ << 16) {
            fprintf(stderr, "%s: bad READ response = %08x, expected %08x\n",
                a->name, response, PE_READ << 16);
            exit(-1);
        }
        for (i=0; i<32; i++) {
            *data++ = get_pe_response(a);           /* Get data */
        }
        addr += 32*4;
    }
}

/*
 * Download programming executive (PE).
 */
static void mpsse_load_executive(adapter_t *adapter,
    const unsigned *pe, unsigned nwords, unsigned pe_version)
{
    mpsse_adapter_t *a = (mpsse_adapter_t*) adapter;

    a->use_executive = 1;
    serial_execution(a);

    if (debug_level > 0)
        fprintf(stderr, "%s: download PE loader\n", a->name);

    /* Step 1. */
    xfer_instruction(a, 0x3c04bf88);    // lui a0, 0xbf88
    xfer_instruction(a, 0x34842000);    // ori a0, 0x2000 - address of BMXCON
    xfer_instruction(a, 0x3c05001f);    // lui a1, 0x1f
    xfer_instruction(a, 0x34a50040);    // ori a1, 0x40   - a1 has 001f0040
    xfer_instruction(a, 0xac850000);    // sw  a1, 0(a0)  - BMXCON initialized

    /* Step 2. */
    xfer_instruction(a, 0x34050800);    // li  a1, 0x800  - a1 has 00000800
    xfer_instruction(a, 0xac850010);    // sw  a1, 16(a0) - BMXDKPBA initialized

    /* Step 3. */
    xfer_instruction(a, 0x8c850040);    // lw  a1, 64(a0) - load BMXDMSZ
    xfer_instruction(a, 0xac850020);    // sw  a1, 32(a0) - BMXDUDBA initialized
    xfer_instruction(a, 0xac850030);    // sw  a1, 48(a0) - BMXDUPBA initialized

    /* Step 4. */
    xfer_instruction(a, 0x3c04a000);    // lui a0, 0xa000
    xfer_instruction(a, 0x34840800);    // ori a0, 0x800  - a0 has a0000800

    /* Download the PE loader. */
    int i;
    for (i=0; i<PIC32_PE_LOADER_LEN; i+=2) {
        /* Step 5. */
        unsigned opcode1 = 0x3c060000 | pic32_pe_loader[i];
        unsigned opcode2 = 0x34c60000 | pic32_pe_loader[i+1];

        xfer_instruction(a, opcode1);       // lui a2, PE_loader_hi++
        xfer_instruction(a, opcode2);       // ori a2, PE_loader_lo++
        xfer_instruction(a, 0xac860000);    // sw  a2, 0(a0)
        xfer_instruction(a, 0x24840004);    // addiu a0, 4
    }

    /* Jump to PE loader (step 6). */
    xfer_instruction(a, 0x3c19a000);    // lui t9, 0xa000
    xfer_instruction(a, 0x37390800);    // ori t9, 0x800  - t9 has a0000800
    xfer_instruction(a, 0x03200008);    // jr  t9
    xfer_instruction(a, 0x00000000);    // nop

    /* Switch from serial to fast execution mode. */
    mpsse_send(a, TMS_HEADER_COMMAND_NBITS, TMS_HEADER_COMMAND_VAL,
                    MTAP_COMMAND_NBITS , TAP_SW_ETAP,
                    TMS_FOOTER_COMMAND_NBITS, TMS_FOOTER_COMMAND_VAL,
                    0);
    /* TMS 1-1-1-1-1-0 */
    mpsse_send(a, TMS_HEADER_RESET_TAP_NBITS, TMS_HEADER_RESET_TAP_VAL,
                0, 0, 0, 0, 0);

    /* Send parameters for the loader (step 7-A).
     * PE_ADDRESS = 0xA000_0900,
     * PE_SIZE */
    /* Send command. */
    mpsse_send(a, TMS_HEADER_COMMAND_NBITS, TMS_HEADER_COMMAND_VAL,
                    ETAP_COMMAND_NBITS, ETAP_FASTDATA, 
                    TMS_FOOTER_COMMAND_NBITS, TMS_FOOTER_COMMAND_VAL,
                    0);  
    xfer_fastdata(a, 0xa0000900);
    xfer_fastdata(a, nwords);

    /* Download the PE itself (step 7-B). */
    if (debug_level > 0)
        fprintf(stderr, "%s: download PE\n", a->name);
    for (i=0; i<nwords; i++) {
        xfer_fastdata(a, *pe++);
    }
    mpsse_flush_output(a);
    mdelay(10);

    /* Download the PE instructions. */
    xfer_fastdata(a, 0);                        /* Step 8 - jump to PE. */
    xfer_fastdata(a, 0xDEAD0000);
    mpsse_flush_output(a);
    mdelay(10);
    xfer_fastdata(a, PE_EXEC_VERSION << 16);

    unsigned version = get_pe_response(a);
    if (version != (PE_EXEC_VERSION << 16 | pe_version)) {
        fprintf(stderr, "%s: bad PE version = %08x, expected %08x\n",
            a->name, version, PE_EXEC_VERSION << 16 | pe_version);
        exit(-1);
    }
    if (debug_level > 0)
        fprintf(stderr, "%s: PE version = %04x\n",
            a->name, version & 0xffff);
}

/*
 * Erase all flash memory.
 */
static void mpsse_erase_chip(adapter_t *adapter)
{
    mpsse_adapter_t *a = (mpsse_adapter_t*) adapter;

    /* Send command. */
    mpsse_send(a, TMS_HEADER_COMMAND_NBITS, TMS_HEADER_COMMAND_VAL,
                    TMS_HEADER_COMMAND_VAL, TAP_SW_MTAP,
                    TMS_FOOTER_COMMAND_NBITS, TMS_FOOTER_COMMAND_VAL,
                    0);
    /* Send command. */
    mpsse_send(a, TMS_HEADER_COMMAND_NBITS, TMS_HEADER_COMMAND_VAL,
                    TMS_HEADER_COMMAND_VAL, MTAP_COMMAND,
                    TMS_FOOTER_COMMAND_NBITS, TMS_FOOTER_COMMAND_VAL,
                    0);
    /* Xfer data. */
    mpsse_send(a, TMS_HEADER_XFERDATA_NBITS, TMS_HEADER_XFERDATA_VAL,
                    MTAP_COMMAND_DR_NBITS, MTAP_COMMAND,
                    TMS_FOOTER_XFERDATA_NBITS, TMS_FOOTER_XFERDATA_VAL,
                    0);
    mpsse_flush_output(a);
    mdelay(400);

    /* Leave it in ETAP mode. */
    /* Send command. */
    mpsse_send(a, TMS_HEADER_COMMAND_NBITS, TMS_HEADER_COMMAND_VAL,
                    MTAP_COMMAND_NBITS , TAP_SW_ETAP,
                    TMS_FOOTER_COMMAND_NBITS, TMS_FOOTER_COMMAND_VAL,
                    0);
}

/*
 * Write a word to flash memory.
 */
static void mpsse_program_word(adapter_t *adapter,
    unsigned addr, unsigned word)
{
    mpsse_adapter_t *a = (mpsse_adapter_t*) adapter;

    if (debug_level > 0)
        fprintf(stderr, "%s: program word at %08x: %08x\n", a->name, addr, word);
    if (! a->use_executive) {
        /* Without PE. */
        fprintf(stderr, "%s: slow flash write not implemented yet.\n", a->name);
        exit(-1);
    }

    /* Use PE to write flash memory. */
    /* Send command. */
    mpsse_send(a, TMS_HEADER_COMMAND_NBITS, TMS_HEADER_COMMAND_VAL,
                    ETAP_COMMAND_NBITS, ETAP_FASTDATA, 
                    TMS_FOOTER_COMMAND_NBITS, TMS_FOOTER_COMMAND_VAL,
                    0);  
    xfer_fastdata(a, PE_WORD_PROGRAM << 16 | 2);
    mpsse_flush_output(a);
    xfer_fastdata(a, addr);                     /* Send address. */
    mpsse_flush_output(a);
    xfer_fastdata(a, word);                     /* Send word. */

    unsigned response = get_pe_response(a);
    if (response != (PE_WORD_PROGRAM << 16)) {
        fprintf(stderr, "%s: failed to program word %08x at %08x, reply = %08x\n",
            a->name, word, addr, response);
        exit(-1);
    }
}

/*
 * Flash write row of memory.
 */
static void mpsse_program_row(adapter_t *adapter, unsigned addr,
    unsigned *data, unsigned words_per_row)
{
    mpsse_adapter_t *a = (mpsse_adapter_t*) adapter;
    int i;

    if (debug_level > 0)
        fprintf(stderr, "%s: row program %u words at %08x\n",
            a->name, words_per_row, addr);
    if (! a->use_executive) {
        /* Without PE. */
        fprintf(stderr, "%s: slow flash write not implemented yet.\n", a->name);
        exit(-1);
    }

    /* Use PE to write flash memory. */
    /* Send command. */
    mpsse_send(a, TMS_HEADER_COMMAND_NBITS, TMS_HEADER_COMMAND_VAL,
                    ETAP_COMMAND_NBITS, ETAP_FASTDATA, 
                    TMS_FOOTER_COMMAND_NBITS, TMS_FOOTER_COMMAND_VAL,
                    0);  
    xfer_fastdata(a, PE_ROW_PROGRAM << 16 | words_per_row);
    mpsse_flush_output(a);
    xfer_fastdata(a, addr);                     /* Send address. */

    /* Download data. */
    for (i = 0; i < words_per_row; i++) {
        if ((i & 7) == 0)
            mpsse_flush_output(a);
        xfer_fastdata(a, *data++);              /* Send word. */
    }
    mpsse_flush_output(a);

    unsigned response = get_pe_response(a);
    if (response != (PE_ROW_PROGRAM << 16)) {
        fprintf(stderr, "%s: failed to program row at %08x, reply = %08x\n",
            a->name, addr, response);
        exit(-1);
    }
}

/*
 * Verify a block of memory.
 */
static void mpsse_verify_data(adapter_t *adapter,
    unsigned addr, unsigned nwords, unsigned *data)
{
    mpsse_adapter_t *a = (mpsse_adapter_t*) adapter;
    unsigned data_crc, flash_crc;

    //fprintf(stderr, "%s: verify %d words at %08x\n", a->name, nwords, addr);
    if (! a->use_executive) {
        /* Without PE. */
        fprintf(stderr, "%s: slow verify not implemented yet.\n", a->name);
        exit(-1);
    }

    /* Use PE to get CRC of flash memory. */
    /* Send command. */
    mpsse_send(a, TMS_HEADER_COMMAND_NBITS, TMS_HEADER_COMMAND_VAL,
                    ETAP_COMMAND_NBITS, ETAP_FASTDATA, 
                    TMS_FOOTER_COMMAND_NBITS, TMS_FOOTER_COMMAND_VAL,
                    0); 
    xfer_fastdata(a, PE_GET_CRC << 16);
    mpsse_flush_output(a);
    xfer_fastdata(a, addr);                     /* Send address. */
    mpsse_flush_output(a);
    xfer_fastdata(a, nwords * 4);               /* Send length. */

    unsigned response = get_pe_response(a);
    if (response != (PE_GET_CRC << 16)) {
        fprintf(stderr, "%s: failed to verify %d words at %08x, reply = %08x\n",
            a->name, nwords, addr, response);
        exit(-1);
    }
    flash_crc = get_pe_response(a) & 0xffff;

    data_crc = calculate_crc(0xffff, (unsigned char*) data, nwords * 4);
    if (flash_crc != data_crc) {
        fprintf(stderr, "%s: checksum failed at %08x: sum=%04x, expected=%04x\n",
            a->name, addr, flash_crc, data_crc);
        //exit(-1);
    }
}

/*
 * Initialize adapter F2232.
 * Return a pointer to a data structure, allocated dynamically.
 * When adapter not found, return 0.
 * Parameters vid, pid and serial are not used.
 */
adapter_t *adapter_open_mpsse(int vid, int pid, const char *serial)
{
    mpsse_adapter_t *a;
    unsigned char product [256];
    int i;

    a = calloc(1, sizeof(*a));
    if (! a) {
        fprintf(stderr, "adapter_open_mpsse: out of memory\n");
        return 0;
    }
    a->context = NULL;
    int ret = libusb_init(&a->context);

    if (ret != 0) {
        fprintf(stderr, "libusb init failed: %d: %s\n",
            ret, libusb_strerror(ret));
        exit(-1);
    }

    for (i = 0; devlist[i].vid; i++) {
        a->usbdev = libusb_open_device_with_vid_pid(a->context, devlist[i].vid, devlist[i].pid);
        if (a->usbdev != NULL) {
            int match = 1;
            if (devlist[i].product != NULL) {

                struct libusb_device_descriptor desc = {0};
                int rc = libusb_get_device_descriptor(libusb_get_device(a->usbdev), &desc);
                if (rc != 0) {
                    match = 0;
                } else {
                    rc = libusb_get_string_descriptor_ascii(a->usbdev, desc.iProduct, product, 256);

                    if (strcmp((const char *)product, devlist[i].product) != 0) {
                        match = 0;
                    }
                }
            }

            if (match == 1) {
                a->name = devlist[i].name;
                a->mhz = devlist[i].mhz;
                a->dir_control      = devlist[i].dir_control;
                a->trst_control     = devlist[i].trst_control;
                a->trst_inverted    = devlist[i].trst_inverted;
                a->sysrst_control   = devlist[i].sysrst_control;
                a->sysrst_inverted  = devlist[i].sysrst_inverted;
                a->led_control      = devlist[i].led_control;
                a->led_inverted     = devlist[i].led_inverted;
                a->extra_output     = devlist[i].extra_output;
                goto found;
            }
        }
    }

    free(a);
    return 0;

found:
    /*fprintf(stderr, "found USB adapter: vid %04x, pid %04x, type %03x\n",
        dev->descriptor.idVendor, dev->descriptor.idProduct,
        dev->descriptor.bcdDevice);*/

    /* Check if driver is already detached */
    ret = libusb_kernel_driver_active(a->usbdev, 0);
	if (ret != 0){
        ret = libusb_detach_kernel_driver(a->usbdev, 0);
        if (ret != 0) {
            fprintf(stderr, "Error detaching kernel driver: %d: %s\n",
                ret, libusb_strerror(ret));
            libusb_close(a->usbdev);
            exit(-1);
        }
    }

    libusb_claim_interface(a->usbdev, 0);

    /* Reset the ftdi device. */
    if (libusb_control_transfer(a->usbdev,
        LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE | LIBUSB_ENDPOINT_OUT,
        SIO_RESET, 0, 1, 0, 0, 1000) != 0) {
        if (errno == EPERM)
            fprintf(stderr, "%s: superuser privileges needed.\n", a->name);
        else
            fprintf(stderr, "%s: FTDI reset failed\n", a->name);
failed: libusb_release_interface(a->usbdev, 0);
        libusb_close(a->usbdev);
        free(a);
        return 0;
    }

    /* MPSSE mode. */
    if (libusb_control_transfer(a->usbdev,
        LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE | LIBUSB_ENDPOINT_OUT,
        SIO_SET_BITMODE, 0x20b, 1, 0, 0, 1000) != 0) {
        fprintf(stderr, "%s: can't set sync mpsse mode\n", a->name);
        goto failed;
    }

    /* Optimal latency timer is 1 for slow mode and 0 for fast mode. */
    unsigned latency_timer = (a->mhz > 6) ? 0 : 1;
    if (libusb_control_transfer(a->usbdev,
        LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE | LIBUSB_ENDPOINT_OUT,
        SIO_SET_LATENCY_TIMER, latency_timer, 1, 0, 0, 1000) != 0) {
        fprintf(stderr, "%s: unable to set latency timer\n", a->name);
        goto failed;
    }
    if (libusb_control_transfer(a->usbdev,
        LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE | LIBUSB_ENDPOINT_IN,
        SIO_GET_LATENCY_TIMER, 0, 1, (unsigned char*) &latency_timer, 1, 1000) != 1) {
        fprintf(stderr, "%s: unable to get latency timer\n", a->name);
        goto failed;
    }
    if (debug_level)
        fprintf(stderr, "%s: latency timer: %u usec\n", a->name, latency_timer);

    /* By default, use 500 kHz speed. */
    int khz = 500;
    mpsse_speed(a, khz);

    /* Disable TDI to TDO loopback. */
    unsigned char enable_loopback[] = "\x85";
    bulk_write(a, enable_loopback, 1);

    /* Activate LED. */
    mpsse_reset(a, 0, 0, 1);

    /* Reset the JTAG TAP controller: TMS 1-1-1-1-1-0.
     * After reset, the IDCODE register is always selected.
     * Read out 32 bits of data. */
    unsigned idcode;
    mpsse_send(a, TMS_HEADER_RESET_TAP_NBITS + TMS_HEADER_XFERDATA_NBITS,
                    TMS_HEADER_RESET_TAP_VAL + (TMS_HEADER_XFERDATA_VAL << TMS_HEADER_RESET_TAP_NBITS),
                    32, 0,
                    TMS_FOOTER_XFERDATA_NBITS, TMS_FOOTER_XFERDATA_VAL,
                    1);
    idcode = mpsse_recv(a);
    if ((idcode & 0xfff) != 0x053) {
        /* Microchip vendor ID is expected. */
        if (debug_level > 0 || (idcode != 0 && idcode != 0xffffffff))
            fprintf(stderr, "%s: incompatible CPU detected, IDCODE=%08x\n",
                a->name, idcode);
        mpsse_reset(a, 0, 0, 0);
        goto failed;
    }

    /* Activate /SYSRST and LED. */
    mpsse_reset(a, 0, 1, 1);
    mdelay(10);

    /* Check status. */
    /* Send command. */
    mpsse_send(a, TMS_HEADER_COMMAND_NBITS, TMS_HEADER_COMMAND_VAL,
                    MTAP_COMMAND_NBITS, TAP_SW_MTAP,
                    TMS_FOOTER_COMMAND_NBITS, TMS_FOOTER_COMMAND_VAL,
                    0);
    /* Send command. */
    mpsse_send(a, TMS_HEADER_COMMAND_NBITS, TMS_HEADER_COMMAND_VAL,
                    MTAP_COMMAND_NBITS, MTAP_COMMAND,
                    TMS_FOOTER_COMMAND_NBITS, TMS_FOOTER_COMMAND_VAL,
                    0);
    /* Xfer data. */
    mpsse_send(a, TMS_HEADER_XFERDATA_NBITS, TMS_HEADER_XFERDATA_VAL,
                    MTAP_COMMAND_DR_NBITS, MCHP_FLASH_ENABLE,
                    TMS_FOOTER_XFERDATA_NBITS, TMS_FOOTER_XFERDATA_VAL,
                    0); 
    /* Xfer data. */
    mpsse_send(a, TMS_HEADER_XFERDATA_NBITS, TMS_HEADER_XFERDATA_VAL,
                    MTAP_COMMAND_DR_NBITS, MCHP_STATUS,
                    TMS_FOOTER_XFERDATA_NBITS, TMS_FOOTER_XFERDATA_VAL,
                    1); 
    unsigned status = mpsse_recv(a);
    if (debug_level > 0)
        fprintf(stderr, "%s: status %04x\n", a->name, status);
    if ((status & ~MCHP_STATUS_DEVRST) !=
        (MCHP_STATUS_CPS | MCHP_STATUS_CFGRDY | MCHP_STATUS_FAEN)) {
        fprintf(stderr, "%s: invalid status = %04x\n", a->name, status);
        mpsse_reset(a, 0, 0, 0);
        goto failed;
    }
    printf("      Adapter: %s\n", a->name);

    a->adapter.block_override = 0;
    a->adapter.flags = (AD_PROBE | AD_ERASE | AD_READ | AD_WRITE);

    /* User functions. */
    a->adapter.close = mpsse_close;
    a->adapter.get_idcode = mpsse_get_idcode;
    a->adapter.load_executive = mpsse_load_executive;
    a->adapter.read_word = mpsse_read_word;
    a->adapter.read_data = mpsse_read_data;
    a->adapter.verify_data = mpsse_verify_data;
    a->adapter.erase_chip = mpsse_erase_chip;
    a->adapter.program_word = mpsse_program_word;
    a->adapter.program_row = mpsse_program_row;
    return &a->adapter;
}
