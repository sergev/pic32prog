/*
 * Interface to PIC32 JTAG port using FT2232-based USB adapter.
 * For example: Olimex ARM-USB-Tiny adapter.
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

typedef struct {
    /* Common part */
    adapter_t adapter;

    /* Device handle for libusb. */
    usb_dev_handle *usbdev;

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
} mpsse_adapter_t;

/*
 * Identifiers of USB adapter.
 */
#define OLIMEX_VID              0x15ba
#define OLIMEX_ARM_USB_TINY     0x0004  /* ARM-USB-Tiny */
#define OLIMEX_ARM_USB_TINY_H   0x002a	/* ARM-USB-Tiny-H */

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

/* Microchip TAP instructions (5-bit). */
#define MTAP_IDCODE             0x01    /* Select chip identification register */
#define MTAP_SW_MTAP            0x04    /* Switch to MCHP TAP controller */
#define MTAP_SW_ETAP            0x05    /* Switch to EJTAG TAP controller */
#define MTAP_COMMAND            0x07    /* Connect to MCHP Command Register */

/* Microchip DR commands (32-bit). */
#define MCHP_STATUS             0x00    /* No action, read status */
#define MCHP_ASSERT_RST         0xd1    /* Assert device reset */
#define MCHP_DEASSERT_RST       0xd0    /* Deassert device reset */
#define MCHP_ERASE              0xfc    /* Flash chip erase */
#define MCHP_FLASH_ENABLE       0xfe    /* Enable access to flash from cpu */
#define MCHP_FLASH_DISABLE      0xfd    /* Disable access to flash from cpu */

/*
 * Send a packet to USB device.
 */
static void bulk_write (mpsse_adapter_t *a, unsigned char *output, int nbytes)
{
    int bytes_written;

    if (debug_level > 1) {
        int i;
        fprintf (stderr, "usb bulk write %d bytes:", nbytes);
        for (i=0; i<nbytes; i++)
            fprintf (stderr, "%c%02x", i ? '-' : ' ', output[i]);
        fprintf (stderr, "\n");
    }
    bytes_written = usb_bulk_write (a->usbdev, IN_EP, (char*) output,
        nbytes, 1000);
    if (bytes_written < 0) {
        fprintf (stderr, "usb bulk write failed\n");
        exit (-1);
    }
    if (bytes_written != nbytes)
        fprintf (stderr, "usb bulk written %d bytes of %d",
            bytes_written, nbytes);
}

/*
 * If there are any data in transmit buffer -
 * send them to device.
 */
static void mpsse_flush_output (mpsse_adapter_t *a)
{
    int bytes_read, n;
    unsigned char reply [64];

    if (a->bytes_to_write <= 0)
        return;

    bulk_write (a, a->output, a->bytes_to_write);
    a->bytes_to_write = 0;
    if (a->bytes_to_read <= 0)
        return;

    /* Get reply. */
    bytes_read = 0;
    while (bytes_read < a->bytes_to_read) {
        n = usb_bulk_read (a->usbdev, OUT_EP, (char*) reply,
            a->bytes_to_read - bytes_read + 2, 2000);
        if (n < 0) {
            fprintf (stderr, "usb bulk read failed\n");
            exit (-1);
        }
        if (debug_level > 1) {
            if (n != a->bytes_to_read + 2)
                fprintf (stderr, "usb bulk read %d bytes of %d\n",
                    n, a->bytes_to_read - bytes_read + 2);
            else {
                int i;
                fprintf (stderr, "usb bulk read %d bytes:", n);
                for (i=0; i<n; i++)
                    fprintf (stderr, "%c%02x", i ? '-' : ' ', reply[i]);
                fprintf (stderr, "\n");
            }
        }
        if (n > 2) {
            /* Copy data. */
            memcpy (a->input + bytes_read, reply + 2, n - 2);
            bytes_read += n - 2;
        }
    }
    if (debug_level > 1) {
        int i;
        fprintf (stderr, "mpsse_flush_output received %d bytes:", a->bytes_to_read);
        for (i=0; i<a->bytes_to_read; i++)
            fprintf (stderr, "%c%02x", i ? '-' : ' ', a->input[i]);
        fprintf (stderr, "\n");
    }
    a->bytes_to_read = 0;
}

static void mpsse_send (mpsse_adapter_t *a,
    unsigned tms_prolog_nbits, unsigned tms_prolog,
    unsigned tdi_nbits, unsigned long long tdi, int read_flag)
{
    unsigned tms_epilog_nbits = 0, tms_epilog = 0;

    if (tdi_nbits > 0) {
        /* We have some data; add generic prologue TMS 1-0-0
         * and epilogue TMS 1-0. */
        tms_prolog |= 1 << tms_prolog_nbits;
        tms_prolog_nbits += 3;
        tms_epilog = 1;
        tms_epilog_nbits = 2;
    }
    /* Check that we have enough space in output buffer.
     * Max size of one packet is 23 bytes (6+8+3+3+3). */
    if (a->bytes_to_write > sizeof (a->output) - 23)
        mpsse_flush_output (a);

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

static unsigned long long mpsse_fix_data (mpsse_adapter_t *a, unsigned long long word)
{
    unsigned long long fix_high_bit = word & a->fix_high_bit;
    //if (debug) fprintf (stderr, "fix (%08llx) high_bit=%08llx\n", word, a->fix_high_bit);

    if (a->high_byte_bits) {
        /* Fix a high byte of received data. */
        unsigned long long high_byte = a->high_byte_mask &
            ((word & a->high_byte_mask) >> (8 - a->high_byte_bits));
        word = (word & ~a->high_byte_mask) | high_byte;
        //if (debug) fprintf (stderr, "Corrected byte %08llx -> %08llx\n", a->high_byte_mask, high_byte);
    }
    word &= a->high_bit_mask - 1;
    if (fix_high_bit) {
        /* Fix a high bit of received data. */
        word |= a->high_bit_mask;
        //if (debug) fprintf (stderr, "Corrected bit %08llx -> %08llx\n", a->high_bit_mask, word >> 9);
    }
    return word;
}

static unsigned long long mpsse_recv (mpsse_adapter_t *a)
{
    unsigned long long word;

    /* Send a packet. */
    mpsse_flush_output (a);

    /* Process a reply: one 64-bit word. */
    memcpy (&word, a->input, sizeof (word));
    return mpsse_fix_data (a, word);
}

static void mpsse_reset (mpsse_adapter_t *a, int trst, int sysrst, int led)
{
    unsigned char output [3];
    unsigned low_output = 0x08; /* TCK idle high */
    unsigned low_direction = 0x1b;
    unsigned high_direction = 0x0f;
    unsigned high_output = 0;

    /* command "set data bits low byte" */
    output [0] = 0x80;
    output [1] = low_output;
    output [2] = low_direction;
    bulk_write (a, output, 3);

    if (! trst)
        high_output |= 1;

    if (sysrst)
        high_output |= 2;

    if (led)
        high_output |= 8;

    /* command "set data bits high byte" */
    output [0] = 0x82;
    output [1] = high_output;
    output [2] = high_direction;

    bulk_write (a, output, 3);
    if (debug_level)
        fprintf (stderr, "mpsse_reset (trst=%d, sysrst=%d) high_output=0x%2.2x, high_direction: 0x%2.2x\n",
            trst, sysrst, high_output, high_direction);
}

static void mpsse_speed (mpsse_adapter_t *a, int divisor)
{
    unsigned char output [3];

    /* command "set TCK divisor" */
    output [0] = 0x86;
    output [1] = divisor;
    output [2] = divisor >> 8;
    bulk_write (a, output, 3);
}

static void mpsse_close (adapter_t *adapter, int power_on)
{
    mpsse_adapter_t *a = (mpsse_adapter_t*) adapter;

    mpsse_flush_output (a);
    mpsse_reset (a, 0, 0, 0);
    usb_release_interface (a->usbdev, 0);
    usb_close (a->usbdev);
    free (a);
}

/*
 * Read the Device Identification code
 */
static unsigned mpsse_get_idcode (adapter_t *adapter)
{
    mpsse_adapter_t *a = (mpsse_adapter_t*) adapter;
    unsigned idcode;

    /* Reset the JTAG TAP controller: TMS 1-1-1-1-1-0.
     * After reset, the IDCODE register is always selected.
     * Read out 32 bits of data. */
    mpsse_send (a, 6, 31, 32, 0, 1);
    idcode = mpsse_recv (a);
    return idcode;
}

/*
 * Read a memory block.
 */
static void mpsse_read_data (adapter_t *adapter,
    unsigned addr, unsigned nwords, unsigned *data)
{
    // TODO
}

/*
 * Download programming executable (PE).
 */
static void mpsse_load_executable (adapter_t *adapter)
{
    // TODO
}

/*
 * Read a word from memory (without PE).
 */
static unsigned mpsse_read_word (adapter_t *adapter, unsigned addr)
{
    // TODO
    return 0;
}

/*
 * Erase all flash memory.
 */
static void mpsse_erase_chip (adapter_t *adapter)
{
    // TODO
}

/*
 * Flash write, 1-kbyte blocks.
 */
static void mpsse_program_block (adapter_t *adapter,
    unsigned addr, unsigned *data)
{
    // TODO
}

/*
 * Write a word to flash memory.
 */
static void mpsse_program_word (adapter_t *adapter,
    unsigned addr, unsigned word)
{
    // TODO
}

/*
 * Initialize adapter F2232.
 * Return a pointer to a data structure, allocated dynamically.
 * When adapter not found, return 0.
 */
adapter_t *adapter_open_mpsse (void)
{
    mpsse_adapter_t *a;
    struct usb_bus *bus;
    struct usb_device *dev;

    usb_init();
    usb_find_busses();
    usb_find_devices();
    for (bus = usb_get_busses(); bus; bus = bus->next) {
        for (dev = bus->devices; dev; dev = dev->next) {
            if (dev->descriptor.idVendor == OLIMEX_VID &&
                (dev->descriptor.idProduct == OLIMEX_ARM_USB_TINY ||
                 dev->descriptor.idProduct == OLIMEX_ARM_USB_TINY_H))
                goto found;
        }
    }
    /*fprintf (stderr, "USB adapter not found: vid=%04x, pid=%04x\n",
        OLIMEX_VID, OLIMEX_PID);*/
    return 0;
found:
    /*fprintf (stderr, "found USB adapter: vid %04x, pid %04x, type %03x\n",
        dev->descriptor.idVendor, dev->descriptor.idProduct,
        dev->descriptor.bcdDevice);*/
    a = calloc (1, sizeof (*a));
    if (! a) {
        fprintf (stderr, "Out of memory\n");
        return 0;
    }
    a->usbdev = usb_open (dev);
    if (! a->usbdev) {
        fprintf (stderr, "MPSSE adapter: usb_open() failed\n");
        free (a);
        return 0;
    }
    usb_claim_interface (a->usbdev, 0);

    /* Reset the ftdi device. */
    if (usb_control_msg (a->usbdev,
        USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_OUT,
        SIO_RESET, 0, 1, 0, 0, 1000) != 0) {
        if (errno == EPERM)
            fprintf (stderr, "MPSSE adapter: superuser privileges needed.\n");
        else
            fprintf (stderr, "MPSSE adapter: FTDI reset failed\n");
failed: usb_release_interface (a->usbdev, 0);
        usb_close (a->usbdev);
        free (a);
        return 0;
    }

    /* MPSSE mode. */
    if (usb_control_msg (a->usbdev,
        USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_OUT,
        SIO_SET_BITMODE, 0x20b, 1, 0, 0, 1000) != 0) {
        fprintf (stderr, "Can't set sync mpsse mode\n");
        goto failed;
    }

    unsigned divisor = 3000;
    unsigned char latency_timer = 100;

    if (usb_control_msg (a->usbdev,
        USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_OUT,
        SIO_SET_LATENCY_TIMER, latency_timer, 1, 0, 0, 1000) != 0) {
        fprintf (stderr, "unable to set latency timer\n");
        goto failed;
    }
    if (usb_control_msg (a->usbdev,
        USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_IN,
        SIO_GET_LATENCY_TIMER, 0, 1, (char*) &latency_timer, 1, 1000) != 1) {
        fprintf (stderr, "unable to get latency timer\n");
        goto failed;
    }
    if (debug_level) {
    	fprintf (stderr, "MPSSE: divisor: %u\n", divisor);
    	fprintf (stderr, "MPSSE: latency timer: %u usec\n", latency_timer);
    }
    mpsse_reset (a, 0, 0, 1);

    if (debug_level) {
     int baud = 6000000 / (divisor + 1);
        fprintf (stderr, "MPSSE: speed %d samples/sec\n", baud);
    }
    mpsse_speed (a, divisor);

    /* Disable TDI to TDO loopback. */
    unsigned char enable_loopback[] = "\x85";
    bulk_write (a, enable_loopback, 1);

    /* Activate /SYSRST. */
    mpsse_reset (a, 0, 1, 1);
    mdelay (10);

    /* Reset the JTAG TAP controller. */

#if 1
    mpsse_send (a, 6, 31, 0, 0, 0);             /* TMS 1-1-1-1-1-0 */
#else
    /* Reset the JTAG TAP controller: TMS 1-1-1-1-1-0.
     * After reset, the IDCODE register is always selected.
     * Read out 32 bits of data. */
    mpsse_send (a, 6, 31, 32, 0, 1);
    unsigned idcode = mpsse_recv (a);
fprintf (stderr, "Idcode = %08x\n", idcode);

    mpsse_send (a, 1, 1, 5, MTAP_IDCODE, 0);    /* Send command. */
    mpsse_send (a, 0, 0, 32, MCHP_STATUS, 1);   /* Xfer data. */
    idcode = mpsse_recv (a);
fprintf (stderr, "Idcode = %08x\n", idcode);
#endif



    mpsse_send (a, 1, 1, 5, MTAP_SW_MTAP, 0);   /* Send command. */
    mpsse_send (a, 1, 1, 5, MTAP_COMMAND, 0);   /* Send command. */

    mpsse_send (a, 0, 0, 32, MCHP_STATUS, 1);   /* Xfer data. */
    unsigned status = mpsse_recv (a);
fprintf (stderr, "Status = %08x\n", status);

    mpsse_send (a, 0, 0, 32, MCHP_STATUS, 1);   /* Xfer data. */
    status = mpsse_recv (a);
fprintf (stderr, "Status = %08x\n", status);

#if 0
    mpsse_send (a, 1, 1, 5, MTAP_SW_MTAP, 0);   /* Send command. */
    mpsse_send (a, 1, 1, 5, MTAP_COMMAND, 0);   /* Send command. */

    mpsse_send (a, 0, 0, 32, MCHP_ERASE, 0);   /* Xfer data. */
    mpsse_send (a, 0, 0, 32, MCHP_STATUS, 1);   /* Xfer data. */
    status = mpsse_recv (a);
fprintf (stderr, "Status = %08x\n", status);
    mdelay (1000);
    mpsse_send (a, 0, 0, 32, MCHP_STATUS, 1);   /* Xfer data. */
    status = mpsse_recv (a);
fprintf (stderr, "Status = %08x\n", status);
#endif
    /* User functions. */
    a->adapter.close = mpsse_close;
    a->adapter.get_idcode = mpsse_get_idcode;
    a->adapter.load_executable = mpsse_load_executable;
    a->adapter.read_word = mpsse_read_word;
    a->adapter.read_data = mpsse_read_data;
    a->adapter.erase_chip = mpsse_erase_chip;
    a->adapter.program_block = mpsse_program_block;
    a->adapter.program_word = mpsse_program_word;
    return &a->adapter;
}
