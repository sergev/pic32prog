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
#if defined(__FreeBSD__) || defined(__DragonFly__)
#   include <libusb.h>
#else
#   include <libusb-1.0/libusb.h>
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
    uint16_t extra_output;      /* Default OE settings, etc. */
    uint16_t icsp_control;      /* Selecting ICSP mode. 0 == JTAG, 1 == ICSP */
    uint16_t icsp_inverted; 
    uint16_t icsp_oe_control;   /* Selecting data direction in ICSP mode. 0 == OUTPUT, 1 == INPUT */
    uint16_t icsp_oe_inverted;
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
    unsigned char input [256];  /* Increased for ICSP, just in case */
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
    unsigned icsp_control, icsp_inverted;
    unsigned icsp_oe_control, icsp_oe_inverted;
    unsigned dir_control;
    unsigned extra_output;

    unsigned mhz;
    unsigned interface;
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

#define TMS_FOOTER_COMMAND_NBITS        2
#define TMS_FOOTER_COMMAND_VAL          0b01
#define TMS_FOOTER_XFERDATA_NBITS       2
#define TMS_FOOTER_XFERDATA_VAL         0b01
#define TMS_FOOTER_XFERDATAFAST_NBITS   2
#define TMS_FOOTER_XFERDATAFAST_VAL     0b01

#define SET_MODE_TAP_RESET  0
#define SET_MODE_EXIT       1
#define SET_MODE_ICSP_SYNC  2

static const device_t devlist[] = {
    { OLIMEX_VID,           OLIMEX_ARM_USB_TINY,    "Olimex ARM-USB-Tiny",               6,  0x0f10, 0x0100, 1,  0x0200,  0,   0x0800,  0, NULL, 0x0000, 0x0100, 1, 0x0008, 1},
    { OLIMEX_VID,           OLIMEX_ARM_USB_TINY_H,  "Olimex ARM-USB-Tiny-H",            30,  0x0f10, 0x0100, 1,  0x0200,  0,   0x0800,  0, NULL, 0x0000, 0x0100, 1, 0x0008, 1},
    { OLIMEX_VID,           OLIMEX_ARM_USB_OCD_H,   "Olimex ARM-USB-OCD-H",             30,  0x0f10, 0x0100, 1,  0x0200,  0,   0x0800,  0, NULL, 0x0000, 0x0100, 1, 0x0008, 1},
    { OLIMEX_VID,           OLIMEX_MIPS_USB_OCD_H,  "Olimex MIPS-USB-OCD-H",            30,  0x0f10, 0x0100, 1,  0x0200,  1,   0x0800,  0, NULL, 0x0000, 0x0100, 1, 0x0008, 1},
    { FTDI_DEFAULT_VID,     FTDI_DEFAULT_PID,       "TinCanTools Flyswatter",            6,  0x0cf0, 0x0010, 1,  0x0020,  1,   0x0c00,  1, "Flyswatter", 0x0000, 0x0100, 1, 0x0008, 1},
    { FTDI_DEFAULT_VID,     FTDI_DEFAULT_PID,       "Neofoxx JTAG/SWD adapter",         30,  0xff3b, 0x0100, 1,  0x0200,  1,   0x8000,  1, "Neofoxx JTAG/SWD adapter", 0x0000, 0x0020, 1, 0x1000, 0},
    { FTDI_DEFAULT_VID,     FTDI_DEFAULT_PID,       "Dangerous Prototypes Bus Blaster", 30,  0x0f10, 0x0100, 1,  0x0200,  1,   0x0000,  0, NULL, 0x0000, 0x0100, 1, 0x0008, 1},
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
    uint64_t icspTemp = 0;

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
            if (INTERFACE_JTAG == a->interface || INTERFACE_DEFAULT == a->interface)
            {
                /* Copy data. */
                memcpy(a->input + bytes_read, reply + 2, n - 2);
            }
            else{
                /* Else ICSP. Do data reconstruction from the bytes */
                int counter;
                for (counter=2; counter<n; counter++){
                    icspTemp = icspTemp>>1 | ((reply[counter]&0x80) ? (1ULL<<63) : 0);
                }
            }
            bytes_read += n - 2;
        }
    }
    if (INTERFACE_ICSP == a->interface){
        /* Since data is LSB, shift data to far right */
        icspTemp = icspTemp>>((63+1) - a->bytes_to_read);
        memcpy(a->input, &icspTemp, sizeof(uint64_t));        
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

/*  Renamed function to mpsse_setPins, since it does more than just reset. 
 *  Now controls state of reset (always SYSRST), LED,
 *  ICSP select and ICSP output enable pins */

static void mpsse_setPins(mpsse_adapter_t *a, int sysrst, int led,
                            int icsp, int icsp_oe, int immediateWrite)
{
    unsigned output    = 0x0008 | a->extra_output;  /* TMS idle high, OE pins etc. */
    unsigned direction = 0x000b | a->dir_control;

    if (sysrst)
        output |= a->sysrst_control;
    if (a->sysrst_inverted)
        output ^= a->sysrst_control;

    if (led)
        output |= a->led_control;
    if (a->led_inverted)
        output ^= a->led_control;

    if (icsp)
        output |= a->icsp_control;
    if (a->icsp_inverted)
        output ^= a->icsp_control;

    if (icsp){
		if (output & a->icsp_oe_control){
            output ^= a->icsp_oe_control; /* Fix for boards that use TMS as OE */
		}
		if (icsp_oe)
		    output |= a->icsp_oe_control;
		if (a->icsp_oe_inverted)
		    output ^= a->icsp_oe_control;
	}

    /* command "set data bits low byte" */
    a->output [a->bytes_to_write++] = 0x80;
    a->output [a->bytes_to_write++] = output;
    a->output [a->bytes_to_write++] = direction;

    /* command "set data bits high byte" */
    a->output [a->bytes_to_write++] = 0x82;
    a->output [a->bytes_to_write++] = output >> 8;
    a->output [a->bytes_to_write++] = direction >> 8;

    if (immediateWrite)
        mpsse_flush_output(a);

    if (debug_level>1)
        fprintf(stderr, "mpsse_setPins(sysrst=%d, led=%d, icsp=%d, icsp_oe=%d)\
                         output=%04x, direction: %04x\n",
                        sysrst, led, icsp, icsp_oe, output, direction);
}

static void mpsse_send(mpsse_adapter_t *a,
    unsigned tms_prolog_nbits, unsigned tms_prolog,
    unsigned tdi_nbits, unsigned long long tdi,
    unsigned tms_epilog_nbits, unsigned tms_epilog, int read_flag)
{

    if (INTERFACE_JTAG == a->interface || INTERFACE_DEFAULT == a->interface)
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
    else{
        /* Else ICSP */
        /* If no read flag, then should be simple.
         *  -> Send two bits, change pin/OE, "read" two bits, change pin/OE
         * If there is read flag, then similar, but from 3rd bit on, read 2 bits.
         *  --> This means, we will get 2 bits of data - one TDO bit and one garbage.
         *   --> Care must be taken to extract the right one!
         *   --> (Since data is shifted LSB, the right one will be the MSB bit in the byte)
         */

        /* Check that we have enough space in output buffer.
         * Max size of one packet is roughly 123 bytes (4+1+32+2+2)*3.
         * Flush in any case, TODO optimize later */
        mpsse_flush_output(a);
        mpsse_setPins(a, 0, 1, 1, 0, 0);   /* No Reset, LED, ICSP, ICSP_OE == Output, not immediate */

        /* Enable 3-phse clocking. Without this, the data will NOT be
         * output on the proper edge! Due to this clock stretching, the speed
         * could be set a bit higher in ICSP, to compensate. */
        a->output [a->bytes_to_write++] = 0x8C;

        /* Write the TMS prologue */
        if (tms_prolog_nbits > 0)
        {
            /* Write all bits. Special case for the last one */
            for(;tms_prolog_nbits>0; tms_prolog_nbits--){
                a->output [a->bytes_to_write++] = BITMODE + LSB + CLKWNEG + WTDI; /* Write in bit mode, LSB first, on TDI, on NEGATIVE EDGE */
                a->output [a->bytes_to_write++] = 2-1;    /* Always write 2 bits - "data" and TMS. */
                a->output [a->bytes_to_write++] = (0) | ((tms_prolog & 0x01)<<1);
                tms_prolog = tms_prolog>>1;

                mpsse_setPins(a, 0, 1, 1, 1, 0);   /* No Reset, LED, ICSP, ICSP_OE == INPUT, not immediate */

                /* During reading, WTDI NEEDS TO BE HERE! Without it won't work properly */
                a->output [a->bytes_to_write++] = (tms_prolog_nbits == 1 && read_flag) ? 
                    (BITMODE + LSB + RTDO + CLKWNEG + WTDI) :  /* Read in bit mode, LSB first, on TDO, on POSITIVE EDGE */
                    (BITMODE + LSB + CLKWNEG + WTDI) ;         
                a->output [a->bytes_to_write++] = 2-1;    /* Always write 2 bits - dummy in this case */
                a->output [a->bytes_to_write++] = 0;
                if (tms_prolog_nbits == 1 && read_flag){
                    a->bytes_to_read++;
                }

                mpsse_setPins(a, 0, 1, 1, 0, 0);   /* No Reset, LED, ICSP, ICSP_OE == OUTPUT, not immediate */
            }
         
            
        }

        /* Write all data bits */
        if (tdi_nbits > 0){
            /* Write all bits. Special case for the last one (TMS = 1)*/
            for(;tdi_nbits>0; tdi_nbits--){
                a->output [a->bytes_to_write++] = BITMODE + LSB + CLKWNEG + WTDI; /* Write in bit mode, LSB first, on TDI, on NEGATIVE EDGE */
                a->output [a->bytes_to_write++] = 2-1;    /* Always write 2 bits - "data" and TMS. */
                a->output [a->bytes_to_write++] = (tdi & 0x01) | ((tdi_nbits == 1) ? 1 : 0)<<1;    /* Write data, then TMS. TMS is 0, except for last bit */
                tdi = tdi>>1;

                mpsse_setPins(a, 0, 1, 1, 1, 0);   /* No Reset, LED, ICSP, ICSP_OE == INPUT, not immediate */

                /* During reading, WTDI NEEDS TO BE HERE! Without it won't work properly */
                a->output [a->bytes_to_write++] = (tdi_nbits > 1 && read_flag) ? /* Don't read on last bit - we got the last bit in the previous one. */
                    (BITMODE + LSB + RTDO + CLKWNEG + WTDI) :  /* Read in bit mode, LSB first, on TDO, on POSITIVE EDGE */
                    (BITMODE + LSB + CLKWNEG + WTDI) ;
                a->output [a->bytes_to_write++] = 2-1;    /* Always write 2 bits - dummy in this case */
                a->output [a->bytes_to_write++] = 0;
                if (tdi_nbits > 1 && read_flag){
                    a->bytes_to_read++;
                }

                mpsse_setPins(a, 0, 1, 1, 0, 0);   /* No Reset, LED, ICSP, ICSP_OE == OUTPUT, not immediate */
            }
        }
    
         /* Write the TMS epilog */
        if (tms_epilog_nbits > 0)
        {
            /* Write all bits. Special case for the last one */
            for(;tms_epilog_nbits>0; tms_epilog_nbits--){
                a->output [a->bytes_to_write++] = BITMODE + LSB + CLKWNEG + WTDI; /* Write in bit mode, LSB first, on TDI, on NEGATIVE EDGE */
                a->output [a->bytes_to_write++] = 2-1;    /* Always write 2 bits - "data" and TMS. */
                a->output [a->bytes_to_write++] = (0) | ((tms_epilog & 0x01)<<1);
                tms_epilog = tms_epilog>>1;

                mpsse_setPins(a, 0, 1, 1, 1, 0);   /* No Reset, LED, ICSP, ICSP_OE == INPUT, not immediate */

                /* During reading, WTDI NEEDS TO BE HERE! Without it won't work properly */
                a->output [a->bytes_to_write++] = (BITMODE + LSB + CLKWNEG + WTDI) ; /* Just read. */
                a->output [a->bytes_to_write++] = 2-1;    /* Always write 2 bits - dummy in this case */
                a->output [a->bytes_to_write++] = 0;

                mpsse_setPins(a, 0, 1, 1, 0, 0);   /* No Reset, LED, ICSP, ICSP_OE == OUTPUT, not immediate */
            }
         
            
        }

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
    if (INTERFACE_JTAG == a->interface || INTERFACE_DEFAULT == a->interface)
        return mpsse_fix_data(a, word);
    else
        /* Else ICSP */
        return(word);
}

static uint32_t mpsse_bitReversal(uint32_t input){
    uint32_t output = 0;
    uint32_t counter = 0;
    for(counter=0; counter<32; counter++){
        if(input & (1<<counter)){
            output = output | (1<<(31-counter));
        }
    }
    return output;
}

static void mpsse_setMode(mpsse_adapter_t *a, uint32_t mode, uint32_t immediate){
    
    if (SET_MODE_TAP_RESET == mode){
        /* TMS 1-1-1-1-1-0 */
        mpsse_send(a, TMS_HEADER_RESET_TAP_NBITS, TMS_HEADER_RESET_TAP_VAL, 0, 0, 0, 0, 0);
    }
    else if (SET_MODE_EXIT == mode){
        /* TMS 1-1-1-1-1 */
        mpsse_send(a, 5, 0x1F, 0, 0, 0, 0, 0);
    }
    else if (SET_MODE_ICSP_SYNC == mode && (INTERFACE_JTAG == a->interface || INTERFACE_DEFAULT == a->interface)){
        // In JTAG mode, send the MCHP key to enter ICSP on the TMS line.
		uint32_t entryCode = 0x4D434850;   // MCHP in ascii.
		entryCode = mpsse_bitReversal(entryCode);
		mpsse_send(a, 8, entryCode & 0xFF, 0, 0, 0, 0, 0);
		mpsse_send(a, 8, (entryCode >> 8) & 0xFF, 0, 0, 0, 0, 0);
		mpsse_send(a, 8, (entryCode >> 16) & 0xFF, 0, 0, 0, 0, 0);
		mpsse_send(a, 8, (entryCode >> 24) & 0xFF, 0, 0, 0, 0, 0);

    }
    else{
        fprintf(stderr, "mpsse_setMode called with invalid mode, quitting\n");
        exit(-1);
    }
    if (immediate){
        mpsse_flush_output(a);
    }
}

static void mpsse_sendCommand(mpsse_adapter_t *a, uint32_t command, uint32_t immediate){
    // All 5-bit commands. The 8-bit ones are command_DR, go through XferData
    if (MTAP_COMMAND != command && TAP_SW_MTAP != command       // MTAP commands
        && TAP_SW_ETAP != command && MTAP_IDCODE != command     // MTAP commands
        && ETAP_ADDRESS != command && ETAP_DATA != command      // ETAP commands
        && ETAP_CONTROL != command && ETAP_EJTAGBOOT != command // ETAP commands
        && ETAP_FASTDATA != command && ETAP_NORMALBOOT != command){ // ETAP commands
        fprintf(stderr, "mpsse_sendCommand called with invalid command 0x%02x, quitting\n", command);
        exit(-1);   // TODO make exit procedure
    }

    if (MTAP_COMMAND != command && TAP_SW_MTAP != command 
        && TAP_SW_ETAP != command && MTAP_IDCODE != command){   // MTAP commands
        mpsse_send(a, TMS_HEADER_COMMAND_NBITS, TMS_HEADER_COMMAND_VAL,
                    MTAP_COMMAND_NBITS, command,
                    TMS_FOOTER_COMMAND_NBITS, TMS_FOOTER_COMMAND_VAL,
                    0);
    }
    else if (ETAP_ADDRESS != command && ETAP_DATA != command    // ETAP commands
            && ETAP_CONTROL != command && ETAP_EJTAGBOOT != command 
            && ETAP_FASTDATA != command && ETAP_NORMALBOOT != command){
        mpsse_send(a, TMS_HEADER_COMMAND_NBITS, TMS_HEADER_COMMAND_VAL,
                    ETAP_COMMAND_NBITS, command,
                    TMS_FOOTER_COMMAND_NBITS, TMS_FOOTER_COMMAND_VAL,
                    0);
    }
    if (immediate){
        mpsse_flush_output(a);
    }

}


static uint64_t mpsse_xferData(mpsse_adapter_t *a, uint32_t nBits, 
                uint32_t iData, uint32_t readFlag, uint32_t immediate){
    mpsse_send(a, TMS_HEADER_XFERDATA_NBITS, TMS_HEADER_XFERDATA_VAL,
                    nBits, iData, 
                    TMS_FOOTER_XFERDATA_NBITS, TMS_FOOTER_XFERDATA_VAL,
                    readFlag);
    if (readFlag){
        /* Flushes the output data, and returns the data */
        return mpsse_recv(a);
    }
    if (immediate){
        mpsse_flush_output(a);
    }
    return 0;
}

static uint64_t mpsse_xferFastData(mpsse_adapter_t *a, unsigned word, uint32_t readFlag, uint32_t immediate)
{
    uint64_t temp;
    mpsse_send(a, TMS_HEADER_XFERDATAFAST_NBITS, TMS_HEADER_XFERDATAFAST_VAL,
                    33, (unsigned long long) word << 1,
                    TMS_FOOTER_XFERDATAFAST_NBITS, TMS_FOOTER_XFERDATAFAST_VAL,
                    1);
    temp = mpsse_recv(a);
    if (!(temp & 0x01)){
        fprintf(stderr, "Warning: PrACC not set in xferFastData\n");
    }
    if (readFlag){
        return temp;
    }
    return 0;
}

static void mpsse_xferInstruction(mpsse_adapter_t *a, unsigned instruction)
{
    unsigned ctl;
    unsigned maxCounter = 0;

    if (debug_level > 1)
        fprintf(stderr, "%s: xfer instruction %08x\n", a->name, instruction);

    /* Select Control Register */
    mpsse_sendCommand(a, ETAP_CONTROL, 1); // ETAP_CONTROL, immediate

    // Wait until CPU is ready
    // Check if Processor Access bit (bit 18) is set
    do {
        ctl = mpsse_xferData(a, 32, (CONTROL_PRACC | CONTROL_PROBEN 
            | CONTROL_PROBTRAP | CONTROL_EJTAGBRK), 1, 1);    // Send data, readflag, immediate don't care
        // For MK family, PRACC doesn't cut it.
//        if ((! (ctl & CONTROL_PRACC))){
        if (!(ctl & CONTROL_PROBEN)){  
            fprintf(stderr, "xfer instruction, ctl was %08x\n", ctl);
            maxCounter++;
            if (maxCounter > 40){
                fprintf(stderr, "Processor still not ready. Quitting\n");
                exit(-1);   // TODO exit procedure
            }
            mdelay(1000);
        }
//    } while (! (ctl & CONTROL_PRACC));
    } while (! (ctl & CONTROL_PROBEN));

    /* Select Data Register */
    mpsse_sendCommand(a, ETAP_DATA, 1);    // ETAP_DATA, immediate

    /* Send the instruction */
    mpsse_xferData(a, 32, instruction, 0, 1);  // Send instruction, don't read, immediate

    /* Tell CPU to execute instruction */
    mpsse_sendCommand(a, ETAP_CONTROL, 1); // ETAP_CONTROL, immediate
    /* Send data. */
    mpsse_xferData(a, 32, (CONTROL_PROBEN | CONTROL_PROBTRAP), 0, 1);   // Send data, no readback, immediate
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

    mpsse_sendCommand(a, TAP_SW_ETAP, 1);  
    mpsse_setMode(a, SET_MODE_TAP_RESET, 1);   // Send TAP reset, immediate
    mdelay(10);

    /* Toggle /SYSRST. */
    mpsse_setPins(a, 1, 1, 0, 0, 1); // Reset, LED, no ICSP, no ICSP_OE, immediate
    mdelay(100);    /* Hold in reset for a bit, so it auto-runs afterwards */
    mpsse_setPins(a, 0, 0, 0, 0, 1); // No Reset, no LED, no ICSP, no ICSP_OE, immediate

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
    mpsse_setMode(a, SET_MODE_TAP_RESET, 1);    /* Send TAP reset, immediate */
    idcode = mpsse_xferData(a, 32, 0, 1, 1);    /* Send 32 0s, receive, immediate (don't care) */
    return idcode;
}

/* Sends the special command to enter ICSP mode */
static void mpsse_enter_icsp(mpsse_adapter_t *a)
{
    uint32_t entryCode = 0x4D434850;   /* MCHP in ascii.
                                        * Data is normally sent LSB first,
                                        * so we need to bit reverse this */
	unsigned tempInterface = a->interface;	// Save, this might come in handy in JTAG too.

    entryCode = mpsse_bitReversal(entryCode);
    a->interface = INTERFACE_JTAG;  /* Sets up mpsse_send with different clocking
                                    -> Data needs to be sent on rising edge,
                                     JTAG mode does that already */
    mpsse_setPins(a, 1, 1, 1, 0, 1);  /* Reset, LED, no ICSP, ICSP_OE = Output, immediate */
    mpsse_flush_output(a);
    mdelay(10);

    mpsse_setPins(a, 0, 1, 1, 0, 1);  /* No Reset, LED, ICSP, ICSP_OE = Output, immediate */
    mpsse_flush_output(a);
    mdelay(10);

    mpsse_setPins(a, 1, 1, 1, 0, 1);  /* Reset, LED, ICSP, ICSP_OE = Output, immediate */
    mpsse_flush_output(a);

    mpsse_send(a, 0, 0, 32, entryCode, 0, 0, 0);    /* Send the entry code  */
    mpsse_flush_output(a);
    mdelay(10);

	if (INTERFACE_ICSP == tempInterface){
		mpsse_setPins(a, 0, 1, 1, 0, 1);  /* No Reset, LED, ICSP, ICSP_OE = Output, immediate ,
		                                ICSP selected from here on. */
	}
	else{
		mpsse_setPins(a, 0, 1, 0, 0, 1);	// Same thing as above, just no ICSP.
	}
    mpsse_flush_output(a);
    a->interface = tempInterface;  /* Sets up mpsse_send with proper clocking again */ 

}

/*
 * Put device to serial execution mode.
 */
static void serial_execution(mpsse_adapter_t *a)
{
    uint32_t counter = 20;
    uint32_t counterPre = 0;

    if (a->serial_execution_mode)
        return;
    a->serial_execution_mode = 1;

    /* Enter serial execution. */
    if (debug_level > 0)
        fprintf(stderr, "%s: enter serial execution\n", a->name);

    /* Send command. */
    mpsse_sendCommand(a, TAP_SW_MTAP, 0);  // Command, not immediate
    /* Reset TAP */
    mpsse_setMode(a, SET_MODE_TAP_RESET, 1);    // Reset TAP, immediate
    /* Send command. */
    mpsse_sendCommand(a, MTAP_COMMAND, 0);
    /* Xfer data. */
    uint64_t status = mpsse_xferData(a, MTAP_COMMAND_DR_NBITS, MCHP_STATUS, 1, 1);
    if(!(status & MCHP_STATUS_CPS)){
        fprintf(stderr, "CPS bit is SET, please erase MCU first. Status: 0x%08x\n", (uint32_t)status);
        exit(-1);
    }

    do{

        if (INTERFACE_ICSP == a->interface){
            mpsse_xferData(a, MTAP_COMMAND_DR_NBITS, MCHP_ASSERT_RST, 0, 1);    // Data, don't read, immediate
        }
        if (INTERFACE_JTAG == a->interface || INTERFACE_DEFAULT == a->interface){
            mpsse_setPins(a, 1, 1, 0, 0, 1);   // Reset, LED, no ICSP, no ICSP_OE, immediate
            mpsse_flush_output(a);
        }

        //mpsse_setMode(a, SET_MODE_TAP_RESET, 1);    // Reset TAP, immediate
        /* Switch to ETAP */
        mpsse_sendCommand(a, TAP_SW_ETAP, 1);   // Send command, immediate
        /* Reset TAP */
        mpsse_setMode(a, SET_MODE_TAP_RESET, 1);    // Reset TAP, immediate
        /* Put CPU in Serial Exec Mode */
        mpsse_sendCommand(a, ETAP_EJTAGBOOT, 1); // Send command, immediate
        
        if (INTERFACE_JTAG == a->interface || INTERFACE_DEFAULT == a->interface){
            mpsse_setPins(a, 0, 1, 0, 0, 1);   // No reset, LED, no ICSP, no ICSP_OE, immediate
            mpsse_flush_output(a);
        }
        else{
            /* Else ICSP */
            /* Send command. */
            mpsse_sendCommand(a, TAP_SW_MTAP, 1);
            /* Reset TAP */
            mpsse_setMode(a, SET_MODE_TAP_RESET, 1);    // Reset TAP, immediate
            /* Send command. */
            mpsse_sendCommand(a, MTAP_COMMAND, 1);
            /* Send command. */
            mpsse_xferData(a, MTAP_COMMAND_DR_NBITS, MCHP_DEASSERT_RST, 0, 1);
            if (FAMILY_MX1 == a->adapter.family_name_short
                || FAMILY_MX3 == a->adapter.family_name_short){
                /* Send command, only for PIC32MX */
                mpsse_xferData(a, MTAP_COMMAND_DR_NBITS, MCHP_FLASH_ENABLE, 0, 1);
            }
            /* Switch to ETAP */
            mpsse_sendCommand(a, TAP_SW_ETAP, 1);
            mpsse_setMode(a, SET_MODE_TAP_RESET, 1);    // Reset TAP, immediate
        }
    
        /* What is the value of ECR, after trying to connect */
        mdelay(10);
        mpsse_setMode(a, SET_MODE_TAP_RESET, 1);
        mpsse_sendCommand(a, TAP_SW_ETAP, 1);
        mpsse_setMode(a, SET_MODE_TAP_RESET, 1);
        mpsse_sendCommand(a, ETAP_CONTROL, 1);

        // At least on the MK chip, the first read is negative. Read again, and it's ok.
        counterPre = 11;
        do{
            status = mpsse_xferData(a, 32, (CONTROL_PRACC | CONTROL_PROBEN | CONTROL_PROBTRAP), 1, 1);    // Send data, readflag, immediate, don't care
        }while(!(status & CONTROL_PROBEN) && counterPre-- > 1); 

//        if (!(status & CONTROL_PRACC)){   
        if (!(status & CONTROL_PROBEN)){   
            fprintf(stderr, "Failed to enter serial execution. Status was %08x\n", (uint32_t)status);
            if (INTERFACE_JTAG == a->interface || INTERFACE_DEFAULT == a->interface){  
                /* For these chips, ICSP & JTAG pins are (sometimes) shared. Can do a trick */
                if (FAMILY_MX1 == a->adapter.family_name_short
                    || FAMILY_MX3 == a->adapter.family_name_short)
                {
				    fprintf(stderr, "In JTAG mode, trying to recover automatically\n");
				    // MCLR is currently 1.
				    // We need to go 0, enter ICSP, go 1, go 0, repeat this do-while loop.
				    // NOTE!! This needs to be done on the TMS LINE! TDI should be held low, probably.
				    mpsse_setPins(a, 1, 1, 0, 0, 1);   // Reset, LED, no ICSP, no ICSP_OE, immediate - immediate here means something else...
               		mpsse_flush_output(a);
				    mdelay(5);
				    
				    mpsse_setMode(a, SET_MODE_ICSP_SYNC, 1);
				    mdelay(5);

				    mpsse_setPins(a, 0, 1, 0, 0, 1);   // no Reset, LED, no ICSP, no ICSP_OE, immediate - immediate here means something else...
               		mpsse_flush_output(a);
				    mdelay(5);
                }
                else{
                    fprintf(stderr, "In JTAG mode, only recovery is through a power-cycle, or reset via ICSP. Quitting.\n");
                    exit(-1);
                }

				// Reset will be asserted in the beginning of the loop again.
				mdelay(100);			
            }
        }
        
    }while(!(status & CONTROL_PROBEN) && counter-- > 1);    // Repeat, until we sucessefully enter serial execution. Extend if necessary, to more than PROBEN!
    
    if (counter == 0){
        fprintf(stderr, "Couldn't enter serial execution, quitting\n");
    }

    mdelay(10);
}

static unsigned get_pe_response(mpsse_adapter_t *a)
{
    unsigned ctl, response;

    // Select Control Register
    /* Send command. */
    mpsse_sendCommand(a, ETAP_CONTROL, 1);	// Command, immediate

    // Wait until CPU is ready
    // Check if Processor Access bit (bit 18) is set
    do {
        ctl = mpsse_xferData(a, 32, (CONTROL_PRACC | CONTROL_PROBEN 
                | CONTROL_PROBTRAP | CONTROL_EJTAGBRK), 1, 1);    // Send data, readflag, immediate don't care
    } while (! (ctl & CONTROL_PRACC));

    // Select Data Register
    // Send the instruction
    /* Send command. */
    mpsse_sendCommand(a, ETAP_DATA, 1);

    /* Get data. */
    response = mpsse_xferData(a, 32, 0, 1, 1);  // Send 32 zeroes, read response, immediate don't care

    // Tell CPU to execute NOP instruction
    /* Send command. */
    mpsse_sendCommand(a, ETAP_CONTROL, 1);
    /* Send data. */
    mpsse_xferData(a, 32, (CONTROL_PROBEN | CONTROL_PROBTRAP), 0, 1);

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
    unsigned word = 0;

    /* Workaround for PIC32MM. If not in serial execution mode yet,
     * read word twice after enering serial execution,
     * as first word will be garbage. */
    unsigned times = (a->serial_execution_mode)?0:1;

    serial_execution(a);
    do{
        if (FAMILY_MX1 == a->adapter.family_name_short
                        || FAMILY_MX3 == a->adapter.family_name_short
                        || FAMILY_MK == a->adapter.family_name_short
                        || FAMILY_MZ == a->adapter.family_name_short)
        {
            //fprintf(stderr, "%s: read word from %08x\n", a->name, addr);

            mpsse_xferInstruction(a, 0x3c13ff20);            // lui s3, FASTDATA_REG_ADDR(31:16)
            mpsse_xferInstruction(a, 0x3c080000 | addr_hi);  // lui t0, addr_hi
            mpsse_xferInstruction(a, 0x35080000 | addr_lo);  // ori t0, addr_lo
            mpsse_xferInstruction(a, 0x8d090000);            // lw t1, 0(t0)
            mpsse_xferInstruction(a, 0xae690000);            // sw t1, 0(s3)
            mpsse_xferInstruction(a, 0);                     // NOP - necessary!

            /* Send command. */
            mpsse_sendCommand(a, ETAP_FASTDATA, 1);
            /* Get fastdata. */    
            /* Send zeroes, read response, immediate don't care. Shift by 1 to get rid of PrACC */
            word = mpsse_xferFastData(a, 0, 1, 1) >> 1;
        }
        else{
            /* Else PIC32MM */
			mpsse_xferInstruction(a, 0xFF2041B3);					// lui s3, FAST_DATA_REG(32:16). Set address of fastdata register
			mpsse_xferInstruction(a, 0x000041A8 | (addr_hi<<16));	// lui t0, DATA_ADDRESS(31:16)
			mpsse_xferInstruction(a, 0x00005108 | (addr_lo<<16));	// ori t0, DATA_ADDRESS(15:0)
	 		mpsse_xferInstruction(a, 0x0000FD28);					// lw t1, 0(t0) - read data
	 		mpsse_xferInstruction(a, 0x0000F933);					// sw t1, 0(s3) - store data to fast register
			mpsse_xferInstruction(a, 0x0c000c00);					// Nop, 2x
			mpsse_xferInstruction(a, 0x0c000c00);					// Nop, 2x, again. Without this (4x nop), you will get garbage after a few bytes!
																	// Extra Nops make it even worse, always get 0s
	 		
			/* Send command. */
			mpsse_sendCommand(a, ETAP_FASTDATA, 1);

			/* Get fastdata. */
			word = mpsse_xferFastData(a, 0, 1, 1) >> 1; // Send zeroes, read response, immediate don't care. Shift by 1 to get rid of PrACC
        }

    }while(times-- > 0);

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

        mpsse_sendCommand(a, ETAP_FASTDATA, 1);
        mpsse_xferFastData(a, PE_READ << 16 | 32, 0, 1);       /* Read 32 words */  // Data, don't read, immediate
        mpsse_xferFastData(a, addr, 0, 1);                     /* Address */        // Data, don't read, immediate

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

    if (a->adapter.family_name_short == FAMILY_MX1
        || a->adapter.family_name_short == FAMILY_MX3
        || a->adapter.family_name_short == FAMILY_MK
        || a->adapter.family_name_short == FAMILY_MZ)
    {
        /* Step 1. */
        mpsse_xferInstruction(a, 0x3c04bf88);    // lui a0, 0xbf88
        mpsse_xferInstruction(a, 0x34842000);    // ori a0, 0x2000 - address of BMXCON
        mpsse_xferInstruction(a, 0x3c05001f);    // lui a1, 0x1f
        mpsse_xferInstruction(a, 0x34a50040);    // ori a1, 0x40   - a1 has 001f0040
        mpsse_xferInstruction(a, 0xac850000);    // sw  a1, 0(a0)  - BMXCON initialized

        /* Step 2. */
        mpsse_xferInstruction(a, 0x34050800);    // li  a1, 0x800  - a1 has 00000800
        mpsse_xferInstruction(a, 0xac850010);    // sw  a1, 16(a0) - BMXDKPBA initialized

        /* Step 3. */
        mpsse_xferInstruction(a, 0x8c850040);    // lw  a1, 64(a0) - load BMXDMSZ
        mpsse_xferInstruction(a, 0xac850020);    // sw  a1, 32(a0) - BMXDUDBA initialized
        mpsse_xferInstruction(a, 0xac850030);    // sw  a1, 48(a0) - BMXDUPBA initialized

        /* Step 4. */
        mpsse_xferInstruction(a, 0x3c04a000);    // lui a0, 0xa000
        mpsse_xferInstruction(a, 0x34840800);    // ori a0, 0x800  - a0 has a0000800

        /* Download the PE loader. */
        int i;
        for (i=0; i<PIC32_PE_LOADER_LEN; i+=2) {
            /* Step 5. */
            unsigned opcode1 = 0x3c060000 | pic32_pe_loader[i];
            unsigned opcode2 = 0x34c60000 | pic32_pe_loader[i+1];

            mpsse_xferInstruction(a, opcode1);       // lui a2, PE_loader_hi++
            mpsse_xferInstruction(a, opcode2);       // ori a2, PE_loader_lo++
            mpsse_xferInstruction(a, 0xac860000);    // sw  a2, 0(a0)
            mpsse_xferInstruction(a, 0x24840004);    // addiu a0, 4
        }

        /* Jump to PE loader (step 6). */
        mpsse_xferInstruction(a, 0x3c19a000);    // lui t9, 0xa000
        mpsse_xferInstruction(a, 0x37390800);    // ori t9, 0x800  - t9 has a0000800
        mpsse_xferInstruction(a, 0x03200008);    // jr  t9
        mpsse_xferInstruction(a, 0x00000000);    // nop

        /* Switch from serial to fast execution mode. */
        mpsse_sendCommand(a, TAP_SW_ETAP, 1);
        /* TMS 1-1-1-1-1-0 */
        mpsse_setMode(a, SET_MODE_TAP_RESET, 1);

        /* Send parameters for the loader (step 7-A).
         * PE_ADDRESS = 0xA000_0900,
         * PE_SIZE */
        /* Send command. */
        mpsse_sendCommand(a, ETAP_FASTDATA, 1);
        mpsse_xferFastData(a, 0xa0000900, 0, 1);    /* Don't read, immediate */
        mpsse_xferFastData(a, nwords, 0, 1);        /* Don't read, immediate */

        /* Download the PE itself (step 7-B). */
        if (debug_level > 0)
            fprintf(stderr, "%s: download PE\n", a->name);
        for (i=0; i<nwords; i++) {
            mpsse_xferFastData(a, *pe++, 0, 0);     /* Don't read, not immediate */
        }
        mpsse_flush_output(a);
        mdelay(10);

        /* Download the PE instructions. */
        /* Step 8 - jump to PE. */
        mpsse_xferFastData(a, 0, 0, 1);             /* Don't read, immediate */                  
        mpsse_xferFastData(a, 0xDEAD0000, 0, 1);    /* Don't read, immediate */
        mdelay(10);
        mpsse_xferFastData(a, PE_EXEC_VERSION << 16, 0, 1); /* Don't read, immediate */
    }
    else{
        /* Else MM family */
        // Step 1. Setup PIC32MM RAM address for the PE 
		mpsse_xferInstruction(a, 0xa00041a4);    // lui a0, 0xa000
		mpsse_xferInstruction(a, 0x02005084);    // ori a0, a0, 0x200 A total of 0xa000_0200

		// Step 2. Load the PE_loader.
		int i;
		for (i=0; i<PIC32_PEMM_LOADER_LEN; i+=2) {
		    /* Step 5. */
		    unsigned opcode1 = 0x41A6 | (pic32_pemm_loader[i] << 16);
		    unsigned opcode2 = 0x50C6 | (pic32_pemm_loader[i+1] << 16);

		    mpsse_xferInstruction(a, opcode1);       // lui a2, PE_loader_hi++
		    mpsse_xferInstruction(a, opcode2);       // ori a2, a2, PE_loader_lo++
		    mpsse_xferInstruction(a, 0x6E42EB40);    // sw  a2, 0(a0); addiu a0, a0, 4;
		}

		// Step 3. Jump to the PE_Loader
		mpsse_xferInstruction(a, 0xA00041B9);       // lui t9, 0xa000
		mpsse_xferInstruction(a, 0x02015339);       // ori t9, t9, 0x0800. Same address as at beginning +1.
		mpsse_xferInstruction(a, 0x0C004599);       // jr t9; nop;

		/* These nops here are MANDATORY. And exactly this many.
		 * Less it doesn't work, more it doesn't work. */
		mpsse_xferInstruction(a, 0x0C000C00);
		mpsse_xferInstruction(a, 0x0C000C00);

		// Step 4. Load the PE using the PE_loader
		// Switch to ETAP
		mpsse_sendCommand(a, TAP_SW_ETAP, 1);
		/* TMS 1-1-1-1-1-0 */
		mpsse_setMode(a, SET_MODE_TAP_RESET, 1);
		// Set to FASTDATA
		mpsse_sendCommand(a, ETAP_FASTDATA, 1);

		// Send PE_ADDRESS, Address os PE program block from PE Hex file
		mpsse_xferFastData(a, 0xA0000300, 0, 1);	// Taken from the .hex file.
		
		// Send PE_SIZE, number as 32-bit words of the program block from the PE Hex file
		mpsse_xferFastData(a, nwords, 0, 1);        // Data, don't read, immediate (wasn't before)

		if (debug_level > 0){
			fprintf(stderr, "%s: download PE, nwords = %d\n", a->name, nwords);
			//mdelay(3000);		
		}
		for (i=0; i<nwords; i++) {
			mpsse_xferFastData(a, *pe++, 0, 0);     // Data, don't read, no immediate
		}
		mpsse_flush_output(a);
		mdelay(10);

		// Step 5, Jump to the PE.
		mpsse_xferFastData(a, 0x00000000, 0, 1);
		mpsse_xferFastData(a, 0xDEAD0000, 0, 1);

		// Done.
		// Get PE version?
		mdelay(10);
		mpsse_xferFastData(a, PE_EXEC_VERSION << 16, 0, 1);     // Data, don't read, immediate (wasn't before)
    }
    
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
    unsigned status = 0;

    /* Switch to MTAP */
    mpsse_sendCommand(a, TAP_SW_MTAP, 1);
    /* Reset TAP */
    mpsse_setMode(a, SET_MODE_TAP_RESET, 1);
    /* Send command. */
    mpsse_sendCommand(a, MTAP_COMMAND, 1);
    /* Xfer data. */
    mpsse_xferData(a, MTAP_COMMAND_DR_NBITS, MCHP_ERASE, 0, 1);
    mpsse_xferData(a, MTAP_COMMAND_DR_NBITS, MCHP_DEASSERT_RST, 0, 1);

    // https://www.microchip.com/forums/m627418.aspx .......
    if (INTERFACE_JTAG == a->interface || INTERFACE_DEFAULT == a->interface){
        mpsse_setPins(a, 0, 1, 0, 0, 1);  /* No Reset, LED, no ICSP, no ICSP_OE, immediate */
    }

    do{
        status = mpsse_xferData(a, MTAP_COMMAND_DR_NBITS, MCHP_STATUS, 1, 1);  // Send data, read response, immediate don't care
        //fprintf(stderr, "Status is 0x%08x ... \n", statuss);
        if (!(status & MCHP_STATUS_CFGRDY) || (status & MCHP_STATUS_FCBUSY)){
            printf(".");
            mdelay(10);
        }
    }while(!(status & MCHP_STATUS_CFGRDY) || (status & MCHP_STATUS_FCBUSY));

    mpsse_setMode(a, SET_MODE_TAP_RESET, 1); 
    mdelay(25);
}

/*
 * Write a word to flash memory.
 */
static void mpsse_program_word(adapter_t *adapter,
    unsigned addr, unsigned word)
{
    mpsse_adapter_t *a = (mpsse_adapter_t*) adapter;

    if (FAMILY_MM == a->adapter.family_name_short){
        fprintf(stderr, "Program word is not available on MM family. Quitting\n");
    }

    if (debug_level > 0)
        fprintf(stderr, "%s: program word at %08x: %08x\n", a->name, addr, word);
    if (! a->use_executive) {
        /* Without PE. */
        fprintf(stderr, "%s: slow flash write not implemented yet.\n", a->name);
        exit(-1);
    }

    /* Use PE to write flash memory. */
    /* Send command. */
    mpsse_sendCommand(a, ETAP_FASTDATA, 1);

    mpsse_xferFastData(a, PE_WORD_PROGRAM << 16 | 2, 0, 1); // Data, don't read, immediate
    mpsse_xferFastData(a, addr, 0, 1);  /* Send address. */ // Data, don't read, immediate
    mpsse_xferFastData(a, word, 0, 1);  /* Send word. */    // Data, don't read, immediate

    unsigned response = get_pe_response(a);
    if (response != (PE_WORD_PROGRAM << 16)) {
        fprintf(stderr, "%s: failed to program word %08x at %08x, reply = %08x\n",
            a->name, word, addr, response);
        exit(-1);
    }
}

static void mpsse_program_double_word(adapter_t *adapter, unsigned addr, unsigned word0, unsigned word1){
    mpsse_adapter_t *a = (mpsse_adapter_t*) adapter;
    
    if (FAMILY_MM != a->adapter.family_name_short){
        fprintf(stderr, "Program double word is only available on MM family. Quitting\n");
    }

	if (debug_level > 0){
		fprintf(stderr, "%s: program double word at 0x%08x: 0x%08x 0x%08x\n", a->name, addr, word0, word1);
	}
	if (! a->use_executive) {
        /* Without PE. */
        fprintf(stderr, "%s: slow flash write not implemented yet.\n", a->name);
        exit(-1);
    }

	 /* Use PE to write flash memory. */
    /* Send command. */
    mpsse_sendCommand(a, ETAP_FASTDATA, 1);
    // TODO - RECHECK the | 2!!!
    mpsse_xferFastData(a, PE_DOUBLE_WORD_PGRM << 16 | 2, 0, 1); // Data, don't read, immediate
    mpsse_xferFastData(a, addr, 0, 1);  	/* Send address. */ // Data, don't read, immediate
    mpsse_xferFastData(a, word0, 0, 1);  	/* Send 1st word. */    // Data, don't read, immediate, was not before
    mpsse_xferFastData(a, word1, 0, 1);  	/* Send 2nd word. */    // Data, don't read, immediate, was not before
 
    unsigned response = get_pe_response(a);
    if (response != (PE_DOUBLE_WORD_PGRM << 16)) {
        fprintf(stderr, "%s: failed to program double words 0x%08x 0x%08x at 0x%08x, reply = %08x\n",
            a->name, word0, word1, addr, response);
        exit(-1);
    }
	
}

static void mpsse_program_quad_word(adapter_t *adapter, unsigned addr, 
            unsigned word0, unsigned word1, unsigned word2, unsigned word3){
    mpsse_adapter_t *a = (mpsse_adapter_t*) adapter;

    if (FAMILY_MK != a->adapter.family_name_short
        && FAMILY_MZ != a->adapter.family_name_short){
        fprintf(stderr, "Program quad word is only available on MK and MZ families. Quitting\n");
    }

	if (debug_level > 0){
		fprintf(stderr, "%s: program quad word at 0x%08x: 0x%08x 0x%08x 0x%08x 0x%08x\n",
                         a->name, addr, word0, word1, word2, word3);
	}
	if (! a->use_executive) {
        /* Without PE. */
        fprintf(stderr, "%s: slow flash write not implemented yet.\n", a->name);
        exit(-1);
    }

	 /* Use PE to write flash memory. */
    /* Send command. */
    mpsse_sendCommand(a, ETAP_FASTDATA, 1);

    mpsse_xferFastData(a, PE_QUAD_WORD_PGRM << 16, 0, 1); // Data, don't read, immediate
    mpsse_xferFastData(a, addr, 0, 1);  	/* Send address. */ // Data, don't read, immediate
    mpsse_xferFastData(a, word0, 0, 1);  	/* Send 1st word. */    // Data, don't read, immediate, was not before
    mpsse_xferFastData(a, word1, 0, 1);  	/* Send 2nd word. */    // Data, don't read, immediate, was not before
    mpsse_xferFastData(a, word2, 0, 1);  	/* Send 1st word. */    // Data, don't read, immediate, was not before
    mpsse_xferFastData(a, word3, 0, 1);  	/* Send 2nd word. */    // Data, don't read, immediate, was not before
 
    unsigned response = get_pe_response(a);
    if (response != (PE_QUAD_WORD_PGRM << 16)) {
        fprintf(stderr, "%s: failed to program quad words 0x%08x 0x%08x 0x%08x 0x%08x at 0x%08x, reply = %08x\n",
            a->name, word0, word1, word2, word3, addr, response);
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
    mpsse_sendCommand(a, ETAP_FASTDATA, 1); 

    mpsse_xferFastData(a, PE_ROW_PROGRAM << 16 | words_per_row, 0, 1);  // Data, don't read, immediate
    mpsse_xferFastData(a, addr, 0, 1);  /* Send address. */             // Data, don't read, immediate

    /* Download data. */
    for (i = 0; i < words_per_row; i++) {
        if ((i & 7) == 0)
            mpsse_flush_output(a);
        mpsse_xferFastData(a, *data++, 0, 0);   /* Send word. Don't read, not immediate */
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
    mpsse_sendCommand(a, ETAP_FASTDATA, 1);

    mpsse_xferFastData(a, PE_GET_CRC << 16, 0, 1);  // Data, don't read, immediate
     /* Send address. */
    mpsse_xferFastData(a, addr, 0, 1);              // Data, don't read, immediate
    /* Send length. */
    mpsse_xferFastData(a, nwords * 4, 0, 1);        // Data, don't read, immediate

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
adapter_t *adapter_open_mpsse(int vid, int pid, const char *serial, 
                                int interface, int speed)
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
                a->interface = interface;
                a->dir_control      = devlist[i].dir_control;
                a->trst_control     = devlist[i].trst_control;
                a->trst_inverted    = devlist[i].trst_inverted;
                a->sysrst_control   = devlist[i].sysrst_control;
                a->sysrst_inverted  = devlist[i].sysrst_inverted;
                a->led_control      = devlist[i].led_control;
                a->led_inverted     = devlist[i].led_inverted;
                a->extra_output     = devlist[i].extra_output;
                a->icsp_control     = devlist[i].icsp_control;
                a->icsp_inverted     = devlist[i].icsp_inverted;
                a->icsp_oe_control     = devlist[i].icsp_oe_control;
                a->icsp_oe_inverted     = devlist[i].icsp_oe_inverted;
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

    /* By default, use 500 kHz speed, unless specified */
    int khz = 500;
    if (0 != speed){
        khz = speed;
    }
    mpsse_speed(a, khz);

    /* Disable TDI to TDO loopback. */
    unsigned char enable_loopback[] = "\x85";
    bulk_write(a, enable_loopback, 1);

    /* Activate LED. */
    mpsse_setPins(a, 0, 1, 0, 0, 1); // No Reset, LED, no ICSP, no ICSP_OE, immediate

    unsigned idcode = 0;
    uint32_t counter = 11;
    do{
        if (INTERFACE_ICSP == interface){
            mpsse_enter_icsp(a);
        }

        /* Delay required for ICSP */
        mdelay(5);     

        /* Reset the JTAG TAP controller: TMS 1-1-1-1-1-0.
         * After reset, the IDCODE register is always selected.
         * Read out 32 bits of data. */
        

        mpsse_setMode(a, SET_MODE_TAP_RESET, 1);
        mpsse_sendCommand(a, TAP_SW_MTAP, 1);
        mpsse_setMode(a, SET_MODE_TAP_RESET, 1);
        mpsse_sendCommand(a, MTAP_IDCODE, 1);

        idcode = mpsse_xferData(a, 32, 0, 1, 1);
        if ((idcode & 0xfff) != 0x053) {
            /* Microchip vendor ID is expected. */
            if (debug_level > 0 || (idcode != 0 && idcode != 0xffffffff))
                fprintf(stderr, "%s: incompatible CPU detected, IDCODE=%08x\n",
                    a->name, idcode);
            fprintf(stderr, "IDCODE not valid, retrying\n");
        }
    }while((idcode & 0xfff) != 0x053 && counter-- > 1);
    if(counter == 0){
        mpsse_setPins(a, 0, 0, 0, 0, 1); // Reset, LED, no ICSP, no ICSP_OE, immediate
        fprintf(stderr, "Couldn't read IDCODE, exiting\n");
        goto failed;
    }
    printf("      IDCODE=%08x\n", idcode);

    /* Activate /SYSRST and LED. Only done in JTAG mode */
    if (INTERFACE_JTAG == a->interface || INTERFACE_DEFAULT == a->interface)
    {
        mpsse_setPins(a, 1, 1, 0, 0, 1); // Reset, LED, no ICSP, no ICSP_OE, immediate

        // So, the MM family's JTAG doesn't work in RESET...      
        // Works like this for all the others as well.  
        mdelay(10);
        mpsse_setPins(a, 0, 1, 0, 0, 1); // No reset, LED, no ICSP, no ICSP_OE, immediate
        
    } 
    mdelay(10);

    /* Check status. */
    /* Send command. */
    mpsse_sendCommand(a, TAP_SW_MTAP, 1);
    /* Send command. */
    mpsse_sendCommand(a, MTAP_COMMAND, 1);
    /* Xfer data. */
    mpsse_xferData(a, MTAP_COMMAND_DR_NBITS, MCHP_FLASH_ENABLE, 0, 1);
    /* Xfer data. */
    unsigned status = mpsse_xferData(a, MTAP_COMMAND_DR_NBITS, MCHP_STATUS, 1, 1);
    
    if (debug_level > 0)
        fprintf(stderr, "%s: status %04x\n", a->name, status);
    if ((status & (MCHP_STATUS_CFGRDY | MCHP_STATUS_FCBUSY)) != (MCHP_STATUS_CFGRDY)) {
        fprintf(stderr, "%s: invalid status = %04x\n", a->name, status);
        mpsse_setPins(a, 0, 0, 0, 0, 1); // No reset, no LED, no ICSP mode, no ICSP_OE, immediate
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
    a->adapter.program_double_word = mpsse_program_double_word;
    a->adapter.program_quad_word = mpsse_program_quad_word;
    return &a->adapter;
}
