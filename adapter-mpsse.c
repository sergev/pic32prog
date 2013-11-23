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
#include <usb.h>

#include "adapter.h"
#include "pic32.h"

typedef struct {
    /* Common part */
    adapter_t adapter;
    const char *name;

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

    /* Mapping of /TRST, /SYSRST and LED control signals. */
    unsigned trst_control, trst_inverted;
    unsigned sysrst_control, sysrst_inverted;
    unsigned led_control, led_inverted;
    unsigned dir_control;

    unsigned mhz;
    unsigned use_executive;
    unsigned serial_execution_mode;
} mpsse_adapter_t;

/*
 * Identifiers of USB adapter.
 */
#define OLIMEX_VID              0x15ba
#define OLIMEX_ARM_USB_TINY     0x0004  /* ARM-USB-Tiny */
#define OLIMEX_ARM_USB_TINY_H   0x002a	/* ARM-USB-Tiny-H */
#define OLIMEX_ARM_USB_OCD_H    0x002b	/* ARM-USB-OCD-H */
#define OLIMEX_MIPS_USB_OCD_H   0x0036	/* MIPS-USB-OCD-H */

#define DP_BUSBLASTER_VID       0x0403
#define DP_BUSBLASTER_PID       0x6010  /* Bus Blaster v2 */

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
        fprintf (stderr, "usb bulk write failed: %d: %s\n",
            bytes_written, usb_strerror());
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
    unsigned char buf [3];
    unsigned output    = 0x0008;                    /* TCK idle high */
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
    bulk_write (a, buf, 3);

    /* command "set data bits high byte" */
    buf [0] = 0x82;
    buf [1] = output >> 8;
    buf [2] = direction >> 8;
    bulk_write (a, buf, 3);

    if (debug_level)
        fprintf (stderr, "mpsse_reset (trst=%d, sysrst=%d) output=%04x, direction: %04x\n",
            trst, sysrst, output, direction);
}

static void mpsse_speed (mpsse_adapter_t *a, int khz)
{
    unsigned char output [3];
    int divisor = (a->mhz * 2000 / khz + 1) / 2 - 1;

    if (divisor < 0)
        divisor = 0;
    if (debug_level)
    	fprintf (stderr, "%s: divisor: %u\n", a->name, divisor);

    if (a->mhz > 6) {
        /* Use 60MHz master clock (disable divide by 5). */
        output [0] = 0x8A;

        /* Turn off adaptive clocking. */
        output [1] = 0x97;

        /* Disable three-phase clocking. */
        output [2] = 0x8D;
        bulk_write (a, output, 3);
    }

    /* Command "set TCK divisor". */
    output [0] = 0x86;
    output [1] = divisor;
    output [2] = divisor >> 8;
    bulk_write (a, output, 3);

    if (debug_level) {
        khz = (a->mhz * 2000 / (divisor + 1) + 1) / 2;
        fprintf (stderr, "%s: clock rate %.1f MHz\n", a->name, khz / 1000.0);
    }
}

static void mpsse_close (adapter_t *adapter, int power_on)
{
    mpsse_adapter_t *a = (mpsse_adapter_t*) adapter;

    /* Clear EJTAGBOOT mode. */
    mpsse_send (a, 1, 1, 5, TAP_SW_ETAP, 0);    /* Send command. */
    mpsse_send (a, 6, 31, 0, 0, 0);             /* TMS 1-1-1-1-1-0 */
    mpsse_flush_output (a);

    /* Toggle /SYSRST. */
    mpsse_reset (a, 0, 1, 1);
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
 * Put device to serial execution mode.
 */
static void serial_execution (mpsse_adapter_t *a)
{
    if (a->serial_execution_mode)
        return;
    a->serial_execution_mode = 1;

    /* Enter serial execution. */
    if (debug_level > 0)
        fprintf (stderr, "%s: enter serial execution\n", a->name);

    mpsse_send (a, 1, 1, 5, TAP_SW_ETAP, 0);    /* Send command. */
    mpsse_send (a, 1, 1, 5, ETAP_EJTAGBOOT, 0); /* Send command. */

    /* Check status. */
    mpsse_send (a, 1, 1, 5, TAP_SW_MTAP, 0);    /* Send command. */
    mpsse_send (a, 1, 1, 5, MTAP_COMMAND, 0);   /* Send command. */
    mpsse_send (a, 0, 0, 8, MCHP_DEASSERT_RST, 0);  /* Xfer data. */
    mpsse_send (a, 0, 0, 8, MCHP_FLASH_ENABLE, 0);  /* Xfer data. */
    mpsse_send (a, 0, 0, 8, MCHP_STATUS, 1);    /* Xfer data. */
    unsigned status = mpsse_recv (a);
    if (debug_level > 0)
        fprintf (stderr, "%s: status %04x\n", a->name, status);
    if ((status & ~MCHP_STATUS_DEVRST) !=
        (MCHP_STATUS_CPS | MCHP_STATUS_CFGRDY | MCHP_STATUS_FAEN)) {
        fprintf (stderr, "%s: invalid status = %04x (reset)\n", a->name, status);
        exit (-1);
    }

    /* Deactivate /SYSRST. */
    mpsse_reset (a, 0, 0, 1);
    mdelay (10);

    /* Check status. */
    mpsse_send (a, 1, 1, 5, TAP_SW_MTAP, 0);    /* Send command. */
    mpsse_send (a, 1, 1, 5, MTAP_COMMAND, 0);   /* Send command. */
    mpsse_send (a, 0, 0, 8, MCHP_STATUS, 1);    /* Xfer data. */
    status = mpsse_recv (a);
    if (debug_level > 0)
        fprintf (stderr, "%s: status %04x\n", a->name, status);
    if (status != (MCHP_STATUS_CPS | MCHP_STATUS_CFGRDY |
                   MCHP_STATUS_FAEN)) {
        fprintf (stderr, "%s: invalid status = %04x (no reset)\n", a->name, status);
        exit (-1);
    }

    /* Leave it in ETAP mode. */
    mpsse_send (a, 1, 1, 5, TAP_SW_ETAP, 0);    /* Send command. */
    mpsse_flush_output (a);
}

static void xfer_fastdata (mpsse_adapter_t *a, unsigned word)
{
    mpsse_send (a, 0, 0, 33, (unsigned long long) word << 1, 0);
}

static void xfer_instruction (mpsse_adapter_t *a, unsigned instruction)
{
    unsigned ctl;

    if (debug_level > 1)
        fprintf (stderr, "%s: xfer instruction %08x\n", a->name, instruction);

    // Select Control Register
    mpsse_send (a, 1, 1, 5, ETAP_CONTROL, 0);       /* Send command. */

    // Wait until CPU is ready
    // Check if Processor Access bit (bit 18) is set
    do {
        mpsse_send (a, 0, 0, 32, CONTROL_PRACC |    /* Xfer data. */
                                 CONTROL_PROBEN |
                                 CONTROL_PROBTRAP |
                                 CONTROL_EJTAGBRK, 1);
        ctl = mpsse_recv (a);
    } while (! (ctl & CONTROL_PRACC));

    // Select Data Register
    // Send the instruction
    mpsse_send (a, 1, 1, 5, ETAP_DATA, 0);          /* Send command. */
    mpsse_send (a, 0, 0, 32, instruction, 0);       /* Send data. */

    // Tell CPU to execute instruction
    mpsse_send (a, 1, 1, 5, ETAP_CONTROL, 0);       /* Send command. */
    mpsse_send (a, 0, 0, 32, CONTROL_PROBEN |       /* Send data. */
                             CONTROL_PROBTRAP, 0);
}

static unsigned get_pe_response (mpsse_adapter_t *a)
{
    unsigned ctl, response;

    // Select Control Register
    mpsse_send (a, 1, 1, 5, ETAP_CONTROL, 0);       /* Send command. */

    // Wait until CPU is ready
    // Check if Processor Access bit (bit 18) is set
    do {
        mpsse_send (a, 0, 0, 32, CONTROL_PRACC |    /* Xfer data. */
                                 CONTROL_PROBEN |
                                 CONTROL_PROBTRAP |
                                 CONTROL_EJTAGBRK, 1);
        ctl = mpsse_recv (a);
    } while (! (ctl & CONTROL_PRACC));

    // Select Data Register
    // Send the instruction
    mpsse_send (a, 1, 1, 5, ETAP_DATA, 0);          /* Send command. */
    mpsse_send (a, 0, 0, 32, 0, 1);                 /* Get data. */
    response = mpsse_recv (a);

    // Tell CPU to execute NOP instruction
    mpsse_send (a, 1, 1, 5, ETAP_CONTROL, 0);       /* Send command. */
    mpsse_send (a, 0, 0, 32, CONTROL_PROBEN |       /* Send data. */
                             CONTROL_PROBTRAP, 0);
    if (debug_level > 1)
        fprintf (stderr, "%s: get PE response %08x\n", a->name, response);
    return response;
}

/*
 * Read a word from memory (without PE).
 */
static unsigned mpsse_read_word (adapter_t *adapter, unsigned addr)
{
    mpsse_adapter_t *a = (mpsse_adapter_t*) adapter;
    unsigned addr_lo = addr & 0xFFFF;
    unsigned addr_hi = (addr >> 16) & 0xFFFF;

    serial_execution (a);

    //fprintf (stderr, "%s: read word from %08x\n", a->name, addr);
    xfer_instruction (a, 0x3c04bf80);           // lui s3, 0xFF20
    xfer_instruction (a, 0x3c080000 | addr_hi); // lui t0, addr_hi
    xfer_instruction (a, 0x35080000 | addr_lo); // ori t0, addr_lo
    xfer_instruction (a, 0x8d090000);           // lw t1, 0(t0)
    xfer_instruction (a, 0xae690000);           // sw t1, 0(s3)

    mpsse_send (a, 1, 1, 5, ETAP_FASTDATA, 0);  /* Send command. */
    mpsse_send (a, 0, 0, 33, 0, 1);             /* Get fastdata. */
    unsigned word = mpsse_recv (a) >> 1;

    if (debug_level > 0)
        fprintf (stderr, "%s: read word at %08x -> %08x\n", a->name, addr, word);
    return word;
}

/*
 * Read a memory block.
 */
static void mpsse_read_data (adapter_t *adapter,
    unsigned addr, unsigned nwords, unsigned *data)
{
    mpsse_adapter_t *a = (mpsse_adapter_t*) adapter;
    unsigned words_read, i;

    //fprintf (stderr, "%s: read %d bytes from %08x\n", a->name, nwords*4, addr);
    if (! a->use_executive) {
        /* Without PE. */
        for (; nwords > 0; nwords--) {
            *data++ = mpsse_read_word (adapter, addr);
            addr += 4;
        }
        return;
    }

    /* Use PE to read memory. */
    for (words_read = 0; words_read < nwords; words_read += 32) {

        mpsse_send (a, 1, 1, 5, ETAP_FASTDATA, 0);
        xfer_fastdata (a, PE_READ << 16 | 32);      /* Read 32 words */
        xfer_fastdata (a, addr);                    /* Address */

        unsigned response = get_pe_response (a);    /* Get response */
        if (response != PE_READ << 16) {
            fprintf (stderr, "%s: bad READ response = %08x, expected %08x\n",
                a->name, response, PE_READ << 16);
            exit (-1);
        }
        for (i=0; i<32; i++) {
            *data++ = get_pe_response (a);          /* Get data */
        }
        addr += 32*4;
    }
}

/*
 * Download programming executive (PE).
 */
static void mpsse_load_executive (adapter_t *adapter,
    const unsigned *pe, unsigned nwords, unsigned pe_version)
{
    mpsse_adapter_t *a = (mpsse_adapter_t*) adapter;

    a->use_executive = 1;
    serial_execution (a);

    if (debug_level > 0)
        fprintf (stderr, "%s: download PE loader\n", a->name);

    /* Step 1. */
    xfer_instruction (a, 0x3c04bf88);   // lui a0, 0xbf88
    xfer_instruction (a, 0x34842000);   // ori a0, 0x2000 - address of BMXCON
    xfer_instruction (a, 0x3c05001f);   // lui a1, 0x1f
    xfer_instruction (a, 0x34a50040);   // ori a1, 0x40   - a1 has 001f0040
    xfer_instruction (a, 0xac850000);   // sw  a1, 0(a0)  - BMXCON initialized

    /* Step 2. */
    xfer_instruction (a, 0x34050800);   // li  a1, 0x800  - a1 has 00000800
    xfer_instruction (a, 0xac850010);   // sw  a1, 16(a0) - BMXDKPBA initialized

    /* Step 3. */
    xfer_instruction (a, 0x8c850040);   // lw  a1, 64(a0) - load BMXDMSZ
    xfer_instruction (a, 0xac850020);   // sw  a1, 32(a0) - BMXDUDBA initialized
    xfer_instruction (a, 0xac850030);   // sw  a1, 48(a0) - BMXDUPBA initialized

    /* Step 4. */
    xfer_instruction (a, 0x3c04a000);   // lui a0, 0xa000
    xfer_instruction (a, 0x34840800);   // ori a0, 0x800  - a0 has a0000800

    /* Download the PE loader. */
    int i;
    for (i=0; i<PIC32_PE_LOADER_LEN; i+=2) {
        /* Step 5. */
        unsigned opcode1 = 0x3c060000 | pic32_pe_loader[i];
        unsigned opcode2 = 0x34c60000 | pic32_pe_loader[i+1];

        xfer_instruction (a, opcode1);      // lui a2, PE_loader_hi++
        xfer_instruction (a, opcode2);      // ori a2, PE_loader_lo++
        xfer_instruction (a, 0xac860000);   // sw  a2, 0(a0)
        xfer_instruction (a, 0x24840004);   // addiu a0, 4
    }

    /* Jump to PE loader (step 6). */
    xfer_instruction (a, 0x3c19a000);   // lui t9, 0xa000
    xfer_instruction (a, 0x37390800);   // ori t9, 0x800  - t9 has a0000800
    xfer_instruction (a, 0x03200008);   // jr  t9
    xfer_instruction (a, 0x00000000);   // nop

    /* Switch from serial to fast execution mode. */
    mpsse_send (a, 1, 1, 5, TAP_SW_ETAP, 0);
    mpsse_send (a, 6, 31, 0, 0, 0);             /* TMS 1-1-1-1-1-0 */

    /* Send parameters for the loader (step 7-A).
     * PE_ADDRESS = 0xA000_0900,
     * PE_SIZE */
    mpsse_send (a, 1, 1, 5, ETAP_FASTDATA, 0);  /* Send command. */
    xfer_fastdata (a, 0xa0000900);
    xfer_fastdata (a, nwords);

    /* Download the PE itself (step 7-B). */
    if (debug_level > 0)
        fprintf (stderr, "%s: download PE\n", a->name);
    for (i=0; i<nwords; i++) {
        xfer_fastdata (a, *pe++);
    }
    mpsse_flush_output (a);
    mdelay (10);

    /* Download the PE instructions. */
    xfer_fastdata (a, 0);                       /* Step 8 - jump to PE. */
    xfer_fastdata (a, 0xDEAD0000);
    mpsse_flush_output (a);
    mdelay (10);
    xfer_fastdata (a, PE_EXEC_VERSION << 16);

    unsigned version = get_pe_response (a);
    if (version != (PE_EXEC_VERSION << 16 | pe_version)) {
        fprintf (stderr, "%s: bad PE version = %08x, expected %08x\n",
            a->name, version, PE_EXEC_VERSION << 16 | pe_version);
        exit (-1);
    }
    if (debug_level > 0)
        fprintf (stderr, "%s: PE version = %04x\n",
            a->name, version & 0xffff);
}

/*
 * Erase all flash memory.
 */
static void mpsse_erase_chip (adapter_t *adapter)
{
    mpsse_adapter_t *a = (mpsse_adapter_t*) adapter;

    mpsse_send (a, 1, 1, 5, TAP_SW_MTAP, 0);    /* Send command. */
    mpsse_send (a, 1, 1, 5, MTAP_COMMAND, 0);   /* Send command. */
    mpsse_send (a, 0, 0, 8, MCHP_ERASE, 0);     /* Xfer data. */
    mpsse_flush_output (a);
    mdelay (400);

    /* Leave it in ETAP mode. */
    mpsse_send (a, 1, 1, 5, TAP_SW_ETAP, 0);    /* Send command. */
}

/*
 * Write a word to flash memory.
 */
static void mpsse_program_word (adapter_t *adapter,
    unsigned addr, unsigned word)
{
    mpsse_adapter_t *a = (mpsse_adapter_t*) adapter;

    if (debug_level > 0)
        fprintf (stderr, "%s: program word at %08x: %08x\n", a->name, addr, word);
    if (! a->use_executive) {
        /* Without PE. */
        fprintf (stderr, "%s: slow flash write not implemented yet.\n", a->name);
        exit (-1);
    }

    /* Use PE to write flash memory. */
    mpsse_send (a, 1, 1, 5, ETAP_FASTDATA, 0);  /* Send command. */
    xfer_fastdata (a, PE_WORD_PROGRAM << 16 | 2);
    mpsse_flush_output (a);
    xfer_fastdata (a, addr);                    /* Send address. */
    mpsse_flush_output (a);
    xfer_fastdata (a, word);                    /* Send word. */

    unsigned response = get_pe_response (a);
    if (response != (PE_WORD_PROGRAM << 16)) {
        fprintf (stderr, "%s: failed to program word %08x at %08x, reply = %08x\n",
            a->name, word, addr, response);
        exit (-1);
    }
}

/*
 * Flash write row of memory.
 */
static void mpsse_program_row (adapter_t *adapter, unsigned addr,
    unsigned *data, unsigned words_per_row)
{
    mpsse_adapter_t *a = (mpsse_adapter_t*) adapter;
    int i;

    if (debug_level > 0)
        fprintf (stderr, "%s: row program %u words at %08x\n",
            a->name, words_per_row, addr);
    if (! a->use_executive) {
        /* Without PE. */
        fprintf (stderr, "%s: slow flash write not implemented yet.\n", a->name);
        exit (-1);
    }

    /* Use PE to write flash memory. */
    mpsse_send (a, 1, 1, 5, ETAP_FASTDATA, 0);  /* Send command. */
    xfer_fastdata (a, PE_ROW_PROGRAM << 16 | words_per_row);
    mpsse_flush_output (a);
    xfer_fastdata (a, addr);                    /* Send address. */

    /* Download data. */
    for (i = 0; i < words_per_row; i++) {
        if ((i & 7) == 0)
            mpsse_flush_output (a);
        xfer_fastdata (a, *data++);             /* Send word. */
    }
    mpsse_flush_output (a);

    unsigned response = get_pe_response (a);
    if (response != (PE_ROW_PROGRAM << 16)) {
        fprintf (stderr, "%s: failed to program row at %08x, reply = %08x\n",
            a->name, addr, response);
        exit (-1);
    }
}

/*
 * Verify a block of memory.
 */
static void mpsse_verify_data (adapter_t *adapter,
    unsigned addr, unsigned nwords, unsigned *data)
{
    mpsse_adapter_t *a = (mpsse_adapter_t*) adapter;
    unsigned data_crc, flash_crc;

    //fprintf (stderr, "%s: verify %d words at %08x\n", a->name, nwords, addr);
    if (! a->use_executive) {
        /* Without PE. */
        fprintf (stderr, "%s: slow verify not implemented yet.\n", a->name);
        exit (-1);
    }

    /* Use PE to get CRC of flash memory. */
    mpsse_send (a, 1, 1, 5, ETAP_FASTDATA, 0);  /* Send command. */
    xfer_fastdata (a, PE_GET_CRC << 16);
    mpsse_flush_output (a);
    xfer_fastdata (a, addr);                    /* Send address. */
    mpsse_flush_output (a);
    xfer_fastdata (a, nwords * 4);              /* Send length. */

    unsigned response = get_pe_response (a);
    if (response != (PE_GET_CRC << 16)) {
        fprintf (stderr, "%s: failed to verify %d words at %08x, reply = %08x\n",
            a->name, nwords, addr, response);
        exit (-1);
    }
    flash_crc = get_pe_response (a) & 0xffff;

    data_crc = calculate_crc (0xffff, (unsigned char*) data, nwords * 4);
    if (flash_crc != data_crc) {
        fprintf (stderr, "%s: checksum failed at %08x: sum=%04x, expected=%04x\n",
            a->name, addr, flash_crc, data_crc);
        //exit (-1);
    }
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
    char product [256];

    a = calloc (1, sizeof (*a));
    if (! a) {
        fprintf (stderr, "adapter_open_mpsse: out of memory\n");
        return 0;
    }
    usb_init();
    usb_find_busses();
    usb_find_devices();
    for (bus = usb_get_busses(); bus; bus = bus->next) {
        for (dev = bus->devices; dev; dev = dev->next) {
            if (dev->descriptor.idVendor == OLIMEX_VID &&
                dev->descriptor.idProduct == OLIMEX_ARM_USB_TINY) {
                a->name = "Olimex ARM-USB-Tiny";
                a->mhz = 6;
                a->dir_control    = 0x0f10;
                a->trst_control   = 0x0100;
                a->trst_inverted  = 1;
                a->sysrst_control = 0x0200;
                a->led_control    = 0x0800;
                goto found;
            }
            if (dev->descriptor.idVendor == OLIMEX_VID &&
                dev->descriptor.idProduct == OLIMEX_ARM_USB_TINY_H) {
                a->name = "Olimex ARM-USB-Tiny-H";
                a->mhz = 30;
                a->dir_control    = 0x0f10;
                a->trst_control   = 0x0100;
                a->trst_inverted  = 1;
                a->sysrst_control = 0x0200;
                a->led_control    = 0x0800;
                goto found;
            }
            if (dev->descriptor.idVendor == OLIMEX_VID &&
                dev->descriptor.idProduct == OLIMEX_ARM_USB_OCD_H) {
                a->name = "Olimex ARM-USB-OCD-H";
                a->mhz = 30;
                a->dir_control     = 0x0f10;
                a->trst_control    = 0x0100;
                a->trst_inverted   = 1;
                a->sysrst_control  = 0x0200;
                a->led_control     = 0x0800;
                goto found;
            }
            if (dev->descriptor.idVendor == OLIMEX_VID &&
                dev->descriptor.idProduct == OLIMEX_MIPS_USB_OCD_H) {
                a->name = "Olimex MIPS-USB-OCD-H";
                a->mhz = 30;
                a->dir_control     = 0x0f10;
                a->trst_control    = 0x0100;
                a->trst_inverted   = 1;
                a->sysrst_control  = 0x0200;
                a->led_control     = 0x0800;
                a->sysrst_inverted = 1;
                goto found;
            }
            if (dev->descriptor.idVendor == DP_BUSBLASTER_VID &&
                dev->descriptor.idProduct == DP_BUSBLASTER_PID) {
                a->name = "Dangerous Prototypes Bus Blaster";
                a->mhz = 30;
                a->dir_control     = 0x0f10;
                a->trst_control    = 0x0100;
                a->trst_inverted   = 1;
                a->sysrst_control  = 0x0200;
                a->sysrst_inverted = 1;
                goto found;
            }
        }
    }
    /*fprintf (stderr, "USB adapter not found: vid=%04x, pid=%04x\n",
        OLIMEX_VID, OLIMEX_PID);*/
    free (a);
    return 0;
found:
    /*fprintf (stderr, "found USB adapter: vid %04x, pid %04x, type %03x\n",
        dev->descriptor.idVendor, dev->descriptor.idProduct,
        dev->descriptor.bcdDevice);*/
    a->usbdev = usb_open (dev);
    if (! a->usbdev) {
        fprintf (stderr, "%s: usb_open() failed\n", a->name);
        free (a);
        return 0;
    }
    if (dev->descriptor.iProduct) {
        if (usb_get_string_simple (a->usbdev, dev->descriptor.iProduct,
                                   product, sizeof(product)) > 0)
        {
            if (strcmp ("Flyswatter", product) == 0) {
                /* TinCanTools Flyswatter.
                 * PID/VID the same as Dangerous Prototypes Bus Blaster. */
                a->name = "TinCanTools Flyswatter";
                a->mhz = 6;
                a->dir_control     = 0x0cf0;
                a->trst_control    = 0x0010;
                a->trst_inverted   = 1;
                a->sysrst_control  = 0x0020;
                a->sysrst_inverted = 0;
                a->led_control     = 0x0c00;
                a->led_inverted    = 1;
            }
        }
    }

#if ! defined (__CYGWIN32__) && ! defined (MINGW32) && ! defined (__APPLE__)
    char driver_name [100];
    if (usb_get_driver_np (a->usbdev, 0, driver_name, sizeof(driver_name)) == 0) {
	if (usb_detach_kernel_driver_np (a->usbdev, 0) < 0) {
            printf("%s: failed to detach the %s kernel driver.\n",
                a->name, driver_name);
            usb_close (a->usbdev);
            free (a);
            return 0;
	}
    }
#endif
    usb_claim_interface (a->usbdev, 0);

    /* Reset the ftdi device. */
    if (usb_control_msg (a->usbdev,
        USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_OUT,
        SIO_RESET, 0, 1, 0, 0, 1000) != 0) {
        if (errno == EPERM)
            fprintf (stderr, "%s: superuser privileges needed.\n", a->name);
        else
            fprintf (stderr, "%s: FTDI reset failed\n", a->name);
failed: usb_release_interface (a->usbdev, 0);
        usb_close (a->usbdev);
        free (a);
        return 0;
    }

    /* MPSSE mode. */
    if (usb_control_msg (a->usbdev,
        USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_OUT,
        SIO_SET_BITMODE, 0x20b, 1, 0, 0, 1000) != 0) {
        fprintf (stderr, "%s: can't set sync mpsse mode\n", a->name);
        goto failed;
    }

    /* Optimal latency timer is 1 for slow mode and 0 for fast mode. */
    unsigned latency_timer = (a->mhz > 6) ? 0 : 1;
    if (usb_control_msg (a->usbdev,
        USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_OUT,
        SIO_SET_LATENCY_TIMER, latency_timer, 1, 0, 0, 1000) != 0) {
        fprintf (stderr, "%s: unable to set latency timer\n", a->name);
        goto failed;
    }
    if (usb_control_msg (a->usbdev,
        USB_TYPE_VENDOR | USB_RECIP_DEVICE | USB_ENDPOINT_IN,
        SIO_GET_LATENCY_TIMER, 0, 1, (char*) &latency_timer, 1, 1000) != 1) {
        fprintf (stderr, "%s: unable to get latency timer\n", a->name);
        goto failed;
    }
    if (debug_level)
    	fprintf (stderr, "%s: latency timer: %u usec\n", a->name, latency_timer);

    /* By default, use 500 kHz speed. */
    int khz = 500;
    mpsse_speed (a, khz);

    /* Disable TDI to TDO loopback. */
    unsigned char enable_loopback[] = "\x85";
    bulk_write (a, enable_loopback, 1);

    /* Activate LED. */
    mpsse_reset (a, 0, 0, 1);

    /* Reset the JTAG TAP controller: TMS 1-1-1-1-1-0.
     * After reset, the IDCODE register is always selected.
     * Read out 32 bits of data. */
    unsigned idcode;
    mpsse_send (a, 6, 31, 32, 0, 1);
    idcode = mpsse_recv (a);
    if ((idcode & 0xfff) != 0x053) {
        /* Microchip vendor ID is expected. */
        if (debug_level > 0 || (idcode != 0 && idcode != 0xffffffff))
            fprintf (stderr, "%s: incompatible CPU detected, IDCODE=%08x\n",
                a->name, idcode);
        mpsse_reset (a, 0, 0, 0);
        goto failed;
    }

    /* Activate /SYSRST and LED. */
    mpsse_reset (a, 0, 1, 1);
    mdelay (10);

    /* Check status. */
    mpsse_send (a, 1, 1, 5, TAP_SW_MTAP, 0);    /* Send command. */
    mpsse_send (a, 1, 1, 5, MTAP_COMMAND, 0);   /* Send command. */
    mpsse_send (a, 0, 0, 8, MCHP_FLASH_ENABLE, 0);  /* Xfer data. */
    mpsse_send (a, 0, 0, 8, MCHP_STATUS, 1);    /* Xfer data. */
    unsigned status = mpsse_recv (a);
    if (debug_level > 0)
        fprintf (stderr, "%s: status %04x\n", a->name, status);
    if ((status & ~MCHP_STATUS_DEVRST) !=
        (MCHP_STATUS_CPS | MCHP_STATUS_CFGRDY | MCHP_STATUS_FAEN)) {
        fprintf (stderr, "%s: invalid status = %04x\n", a->name, status);
        mpsse_reset (a, 0, 0, 0);
        goto failed;
    }
    printf ("      Adapter: %s\n", a->name);

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
