/*
 * Interface to PIC32 ICSP port using Microchip PICkit2 USB adapter.
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
#include "pickit2.h"
#include "pic32.h"

typedef struct {
    /* Common part */
    adapter_t adapter;

    /* Device handle for libusb. */
    usb_dev_handle *usbdev;

    unsigned char reply [64];
    unsigned use_executable;
    unsigned serial_execution_mode;

} pickit2_adapter_t;

/*
 * Identifiers of USB adapter.
 */
#define MICROCHIP_VID           0x04d8
#define PICKIT2_PID             0x0033  /* Microchip PICkit 2 */

/*
 * USB endpoints.
 */
#define OUT_EP                  0x01
#define IN_EP                   0x81

#define IFACE                   0
#define TIMO_MSEC               1000

static void pickit2_send_buf (pickit2_adapter_t *a, unsigned char *buf, unsigned nbytes)
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
    if (usb_interrupt_write (a->usbdev, OUT_EP, (char*) buf, 64, TIMO_MSEC) < 0) {
        fprintf (stderr, "PICkit2: error sending packet\n");
        usb_release_interface (a->usbdev, IFACE);
        usb_close (a->usbdev);
        exit (-1);
    }
}

static void pickit2_send (pickit2_adapter_t *a, unsigned argc, ...)
{
    va_list ap;
    unsigned i;
    unsigned char buf [64];

    memset (buf, CMD_END_OF_BUFFER, 64);
    va_start (ap, argc);
    for (i=0; i<argc; ++i)
        buf[i] = va_arg (ap, int);
    va_end (ap);
    pickit2_send_buf (a, buf, i);
}

static void pickit2_recv (pickit2_adapter_t *a)
{
    if (usb_interrupt_read (a->usbdev, IN_EP, (char*) a->reply,
            64, TIMO_MSEC) != 64) {
        fprintf (stderr, "PICkit2: error receiving packet\n");
        usb_release_interface (a->usbdev, IFACE);
        usb_close (a->usbdev);
        exit (-1);
    }
    if (debug_level > 0) {
        int k;
        fprintf (stderr, "--->>>>");
        for (k=0; k<64; ++k) {
            if (k != 0 && (k & 15) == 0)
                fprintf (stderr, "\n       ");
            fprintf (stderr, " %02x", a->reply[k]);
        }
        fprintf (stderr, "\n");
    }
}

static void check_timeout (pickit2_adapter_t *a, const char *message)
{
    unsigned status;

    pickit2_send (a, 2, CMD_READ_STATUS);
    pickit2_recv (a);
    status = a->reply[0] | a->reply[1] << 8;
    if (status & STATUS_ICD_TIMEOUT) {
        fprintf (stderr, "PICkit2: timed out at %s, status = %04x\n",
            message, status);
        exit (-1);
    }
}

/*
 * Put device to serial execution mode.
 */
static void serial_execution (pickit2_adapter_t *a)
{
    if (a->serial_execution_mode)
        return;
    a->serial_execution_mode = 1;

    // Enter serial execution.
    if (debug_level > 0)
        fprintf (stderr, "PICkit2: enter serial execution\n");
    pickit2_send (a, 29, CMD_EXECUTE_SCRIPT, 27,
        SCRIPT_JT2_SENDCMD, TAP_SW_MTAP,
        SCRIPT_JT2_SENDCMD, MTAP_COMMAND,
        SCRIPT_JT2_XFERDATA8_LIT, MCHP_STATUS,
        SCRIPT_JT2_SENDCMD, TAP_SW_MTAP,
        SCRIPT_JT2_SENDCMD, MTAP_COMMAND,
        SCRIPT_JT2_XFERDATA8_LIT, MCHP_ASSERT_RST,
        SCRIPT_JT2_SENDCMD, TAP_SW_ETAP,
        SCRIPT_JT2_SETMODE, 6, 0x1F,
        SCRIPT_JT2_SENDCMD, ETAP_EJTAGBOOT,
        SCRIPT_JT2_SENDCMD, TAP_SW_MTAP,
        SCRIPT_JT2_SENDCMD, MTAP_COMMAND,
        SCRIPT_JT2_XFERDATA8_LIT, MCHP_DEASSERT_RST,
        SCRIPT_JT2_XFERDATA8_LIT, MCHP_FLASH_ENABLE);
}

/*
 * Download programming executable (PE).
 */
static void pickit2_load_executable (adapter_t *adapter)
{
    pickit2_adapter_t *a = (pickit2_adapter_t*) adapter;

    //fprintf (stderr, "PICkit2: load_executable\n");
    a->use_executable = 1;
    serial_execution (a);

#define WORD_AS_BYTES(w)  (unsigned char) (w), \
                          (unsigned char) ((w) >> 8), \
                          (unsigned char) ((w) >> 16), \
                          (unsigned char) ((w) >> 24)

    if (debug_level > 0)
        fprintf (stderr, "PICkit2: download PE loader\n");
    pickit2_send (a, 45, CMD_CLEAR_DOWNLOAD_BUFFER,
        CMD_DOWNLOAD_DATA, 28,
            WORD_AS_BYTES (0x3c04bf88),         // step 1
            WORD_AS_BYTES (0x34842000),
            WORD_AS_BYTES (0x3c05001f),
            WORD_AS_BYTES (0x34a50040),
            WORD_AS_BYTES (0xac850000),
            WORD_AS_BYTES (0x34050800),         // step 2
            WORD_AS_BYTES (0xac850010),
        CMD_EXECUTE_SCRIPT, 12,                 // execute
            SCRIPT_JT2_SENDCMD, TAP_SW_ETAP,
            SCRIPT_JT2_SETMODE, 6, 0x1F,
            SCRIPT_JT2_XFERINST_BUF,
            SCRIPT_JT2_XFERINST_BUF,
            SCRIPT_JT2_XFERINST_BUF,
            SCRIPT_JT2_XFERINST_BUF,
            SCRIPT_JT2_XFERINST_BUF,
            SCRIPT_JT2_XFERINST_BUF,
            SCRIPT_JT2_XFERINST_BUF);
    check_timeout (a, "step1");

    pickit2_send (a, 30, CMD_CLEAR_DOWNLOAD_BUFFER,
        CMD_DOWNLOAD_DATA, 20,
            WORD_AS_BYTES (0x34058000),         // step 3
            WORD_AS_BYTES (0xac850020),
            WORD_AS_BYTES (0xac850030),
            WORD_AS_BYTES (0x3c04a000),         // step 4
            WORD_AS_BYTES (0x34840800),
        CMD_EXECUTE_SCRIPT, 5,                  // execute
            SCRIPT_JT2_XFERINST_BUF,
            SCRIPT_JT2_XFERINST_BUF,
            SCRIPT_JT2_XFERINST_BUF,
            SCRIPT_JT2_XFERINST_BUF,
            SCRIPT_JT2_XFERINST_BUF);
    check_timeout (a, "step3");

    // Download the PE loader
    int i;
    for (i=0; i<PIC32_PE_LOADER_LEN; i+=2) {
        pickit2_send (a, 25, CMD_CLEAR_DOWNLOAD_BUFFER,
            CMD_DOWNLOAD_DATA, 16,
                WORD_AS_BYTES ((0x3c060000 | pic32_pe_loader[i])),  // step 5
                WORD_AS_BYTES ((0x34c60000 | pic32_pe_loader[i+1])),
                WORD_AS_BYTES (0xac860000),
                WORD_AS_BYTES (0x24840004),
            CMD_EXECUTE_SCRIPT, 4,              // execute
                SCRIPT_JT2_XFERINST_BUF,
                SCRIPT_JT2_XFERINST_BUF,
                SCRIPT_JT2_XFERINST_BUF,
                SCRIPT_JT2_XFERINST_BUF);
        check_timeout (a, "step5");
    }

    // Jump to PE loader
    pickit2_send (a, 42, CMD_CLEAR_DOWNLOAD_BUFFER,
        CMD_DOWNLOAD_DATA, 16,
            WORD_AS_BYTES (0x3c19a000),         // step 6
            WORD_AS_BYTES (0x37390800),
            WORD_AS_BYTES (0x03200008),
            WORD_AS_BYTES (0x00000000),
        CMD_EXECUTE_SCRIPT, 21,                 // execute
            SCRIPT_JT2_XFERINST_BUF,
            SCRIPT_JT2_XFERINST_BUF,
            SCRIPT_JT2_XFERINST_BUF,
            SCRIPT_JT2_XFERINST_BUF,
            SCRIPT_JT2_SENDCMD, TAP_SW_ETAP,
            SCRIPT_JT2_SETMODE, 6, 0x1F,
            SCRIPT_JT2_SENDCMD, ETAP_FASTDATA,
            SCRIPT_JT2_XFRFASTDAT_LIT,
                0, 9, 0, 0xA0,                  // PE_ADDRESS = 0xA000_0900
            SCRIPT_JT2_XFRFASTDAT_LIT,
                (unsigned char) PIC32_PE_LEN,   // PE_SIZE
                (unsigned char) (PIC32_PE_LEN >> 8),
                0, 0);
    check_timeout (a, "step6");

    // Download the PE itself (step 7-B)
    if (debug_level > 0)
        fprintf (stderr, "PICkit2: download PE\n");
    int nloops = (PIC32_PE_LEN + 9) / 10;
    for (i=0; i<nloops; i++) {                  // download 10 at a time
        int j = i * 10;
        pickit2_send (a, 55, CMD_CLEAR_DOWNLOAD_BUFFER,
            CMD_DOWNLOAD_DATA, 40,
                WORD_AS_BYTES (pic32_pe[j]),
                WORD_AS_BYTES (pic32_pe[j+1]),
                WORD_AS_BYTES (pic32_pe[j+2]),
                WORD_AS_BYTES (pic32_pe[j+3]),
                WORD_AS_BYTES (pic32_pe[j+4]),
                WORD_AS_BYTES (pic32_pe[j+5]),
                WORD_AS_BYTES (pic32_pe[j+6]),
                WORD_AS_BYTES (pic32_pe[j+7]),
                WORD_AS_BYTES (pic32_pe[j+8]),
                WORD_AS_BYTES (pic32_pe[j+9]),
            CMD_EXECUTE_SCRIPT, 10,             // execute
                SCRIPT_JT2_XFRFASTDAT_BUF,
                SCRIPT_JT2_XFRFASTDAT_BUF,
                SCRIPT_JT2_XFRFASTDAT_BUF,
                SCRIPT_JT2_XFRFASTDAT_BUF,
                SCRIPT_JT2_XFRFASTDAT_BUF,
                SCRIPT_JT2_XFRFASTDAT_BUF,
                SCRIPT_JT2_XFRFASTDAT_BUF,
                SCRIPT_JT2_XFRFASTDAT_BUF,
                SCRIPT_JT2_XFRFASTDAT_BUF,
                SCRIPT_JT2_XFRFASTDAT_BUF);
        check_timeout (a, "step7");
    }
    mdelay (100);

    // Download the PE instructions
    pickit2_send (a, 15, CMD_CLEAR_DOWNLOAD_BUFFER,
        CMD_DOWNLOAD_DATA, 8,
            WORD_AS_BYTES (0x00000000),         // step 8 - jump to PE
            WORD_AS_BYTES (0xDEAD0000),
        CMD_EXECUTE_SCRIPT, 2,                  // execute
            SCRIPT_JT2_XFRFASTDAT_BUF,
            SCRIPT_JT2_XFRFASTDAT_BUF);
    check_timeout (a, "step8");
    mdelay (100);

    pickit2_send (a, 11, CMD_CLEAR_UPLOAD_BUFFER,
        CMD_EXECUTE_SCRIPT, 8,
            SCRIPT_JT2_SENDCMD, ETAP_FASTDATA,
            SCRIPT_JT2_XFRFASTDAT_LIT,
                0x00, 0x00,                     // length = 0
                0x07, 0x00,                     // EXEC_VERSION
            SCRIPT_JT2_GET_PE_RESP);
    check_timeout (a, "EXEC_VERSION");
    pickit2_send (a, 1, CMD_UPLOAD_DATA);
    pickit2_recv (a);

    unsigned version = a->reply[3] | (a->reply[4] << 8);
    if (version != 0x0007) {                    // command echo
        fprintf (stderr, "PICkit2: bad PE reply = %04x\n", version);
        exit (-1);
    }
    version = a->reply[1] | (a->reply[2] << 8);
    if (version != PIC32_PE_VERSION) {
        fprintf (stderr, "PICkit2: bad PE version = %04x, expected %04x\n",
            version, PIC32_PE_VERSION);
        exit (-1);
    }
    if (debug_level > 0)
        fprintf (stderr, "PICkit2: PE version = %04x\n", version);
}

#if 0
int pe_blank_check (pickit2_adapter_t *a,
    unsigned int start, unsigned int nbytes)
{
    pickit2_send (a, 21, CMD_CLEAR_UPLOAD_BUFFER, CMD_EXECUTE_SCRIPT, 18,
        SCRIPT_JT2_SENDCMD, ETAP_FASTDATA,
        SCRIPT_JT2_XFRFASTDAT_LIT,
            0x00, 0x00,
            0x06, 0x00,                         // BLANK_CHECK
        SCRIPT_JT2_XFRFASTDAT_LIT,
            (unsigned char) start,
            (unsigned char) (start >> 8),
            (unsigned char) (start >> 16),
            (unsigned char) (start >> 24),
        SCRIPT_JT2_XFRFASTDAT_LIT,
            (unsigned char) nbytes,
            (unsigned char) (nbytes >> 8),
            (unsigned char) (nbytes >> 16),
            (unsigned char) (nbytes >> 24),
        SCRIPT_JT2_GET_PE_RESP);
    check_timeout (a, "BLANK_CHECK");
    pickit2_send (a, 1, CMD_UPLOAD_DATA);
    pickit2_recv (a);
    if (a->reply[3] != 6 || a->reply[1] != 0) { // response code 0 = success
        return 0;
    }
    return 1;
}

int pe_get_crc (pickit2_adapter_t *a,
    unsigned int start, unsigned int nbytes)
{
    pickit2_send (a, 22, CMD_CLEAR_UPLOAD_BUFFER, CMD_EXECUTE_SCRIPT, 19,
        SCRIPT_JT2_SENDCMD, ETAP_FASTDATA,
        SCRIPT_JT2_XFRFASTDAT_LIT,
            0x00, 0x00,
            0x08, 0x00,                         // GET_CRC
        SCRIPT_JT2_XFRFASTDAT_LIT,
            (unsigned char) start,
            (unsigned char) (start >> 8),
            (unsigned char) (start >> 16),
            (unsigned char) (start >> 24),
        SCRIPT_JT2_XFRFASTDAT_LIT,
            (unsigned char) nbytes,
            (unsigned char) (nbytes >> 8),
            (unsigned char) (nbytes >> 16),
            (unsigned char) (nbytes >> 24),
        SCRIPT_JT2_GET_PE_RESP,
        SCRIPT_JT2_GET_PE_RESP);
    check_timeout (a, "GET_CRC");
    pickit2_send (a, 1, CMD_UPLOAD_DATA);
    pickit2_recv (a);
    if (a->reply[3] != 8 || a->reply[1] != 0) { // response code 0 = success
        return 0;
    }

    int crc = a->reply[5] | (a->reply[6] << 8);
    return crc;
}
#endif

static void pickit2_finish (pickit2_adapter_t *a, int power_on)
{
    /* Exit programming mode. */
    pickit2_send (a, 18, CMD_CLEAR_UPLOAD_BUFFER, CMD_EXECUTE_SCRIPT, 15,
        SCRIPT_JT2_SETMODE, 5, 0x1f,
        SCRIPT_VPP_OFF,
        SCRIPT_MCLR_GND_ON,
        SCRIPT_VPP_PWM_OFF,
        SCRIPT_SET_ICSP_PINS, 6,
        SCRIPT_SET_ICSP_PINS, 2,
        SCRIPT_SET_ICSP_PINS, 3,
        SCRIPT_DELAY_LONG, 10,                  // 50 msec
        SCRIPT_BUSY_LED_OFF);

    if (! power_on) {
        /* Detach power from the board. */
        pickit2_send (a, 4, CMD_EXECUTE_SCRIPT, 2,
            SCRIPT_VDD_OFF,
            SCRIPT_VDD_GND_ON);
    }

    /* Disable reset. */
    pickit2_send (a, 3, CMD_EXECUTE_SCRIPT, 1,
        SCRIPT_MCLR_GND_OFF);

    /* Read board status. */
    check_timeout (a, "finish");
}

static void pickit2_close (adapter_t *adapter, int power_on)
{
    pickit2_adapter_t *a = (pickit2_adapter_t*) adapter;
    //fprintf (stderr, "PICkit2: close\n");

    pickit2_finish (a, power_on);
    usb_release_interface (a->usbdev, IFACE);
    usb_close (a->usbdev);
    free (a);
}

/*
 * Read the Device Identification code
 */
static unsigned pickit2_get_idcode (adapter_t *adapter)
{
    pickit2_adapter_t *a = (pickit2_adapter_t*) adapter;
    unsigned idcode;

    /* Read device id. */
    pickit2_send (a, 12, CMD_CLEAR_UPLOAD_BUFFER, CMD_EXECUTE_SCRIPT, 9,
        SCRIPT_JT2_SENDCMD, TAP_SW_MTAP,
        SCRIPT_JT2_SENDCMD, MTAP_IDCODE,
        SCRIPT_JT2_XFERDATA32_LIT, 0, 0, 0, 0);
    pickit2_send (a, 1, CMD_UPLOAD_DATA);
    pickit2_recv (a);
    //fprintf (stderr, "PICkit2: read id, %d bytes: %02x %02x %02x %02x\n",
    //  a->reply[0], a->reply[1], a->reply[2], a->reply[3], a->reply[4]);
    if (a->reply[0] != 4)
        return 0;
    idcode = a->reply[1] | a->reply[2] << 8 | a->reply[3] << 16 | a->reply[4] << 24;
    return idcode;
}

/*
 * Read a word from memory (without PE).
 */
static unsigned pickit2_read_word (adapter_t *adapter, unsigned addr)
{
    pickit2_adapter_t *a = (pickit2_adapter_t*) adapter;
    serial_execution (a);

    unsigned addr_lo = addr & 0xFFFF;
    unsigned addr_hi = (addr >> 16) & 0xFFFF;
    pickit2_send (a, 64, CMD_CLEAR_DOWNLOAD_BUFFER,
        CMD_CLEAR_UPLOAD_BUFFER,
        CMD_DOWNLOAD_DATA, 28,
            WORD_AS_BYTES (0x3c04bf80),             // lui s3, 0xFF20
            WORD_AS_BYTES (0x3c080000 | addr_hi),   // lui t0, addr_hi
            WORD_AS_BYTES (0x35080000 | addr_lo),   // ori t0, addr_lo
            WORD_AS_BYTES (0x8d090000),             // lw t1, 0(t0)
            WORD_AS_BYTES (0xae690000),             // sw t1, 0(s3)
            WORD_AS_BYTES (0x00094842),             // srl t1, 1
            WORD_AS_BYTES (0xae690004),             // sw t1, 4(s3)
        CMD_EXECUTE_SCRIPT, 29,
            SCRIPT_JT2_SENDCMD, TAP_SW_ETAP,
            SCRIPT_JT2_SETMODE, 6, 0x1F,
            SCRIPT_JT2_XFERINST_BUF,
            SCRIPT_JT2_XFERINST_BUF,
            SCRIPT_JT2_XFERINST_BUF,
            SCRIPT_JT2_XFERINST_BUF,
            SCRIPT_JT2_XFERINST_BUF,
            SCRIPT_JT2_SENDCMD, ETAP_FASTDATA,      // read FastData
            SCRIPT_JT2_XFERDATA32_LIT, 0, 0, 0, 0,
            SCRIPT_JT2_SETMODE, 6, 0x1F,
            SCRIPT_JT2_XFERINST_BUF,
            SCRIPT_JT2_XFERINST_BUF,
            SCRIPT_JT2_SENDCMD, ETAP_FASTDATA,      // read FastData
            SCRIPT_JT2_XFERDATA32_LIT, 0, 0, 0, 0,
        CMD_UPLOAD_DATA);
    pickit2_recv (a);
    if (a->reply[0] != 8) {
        fprintf (stderr, "PICkit2: read word %08x: bad reply length=%u\n",
            addr, a->reply[0]);
        exit (-1);
    }
    unsigned value = a->reply[1] | (a->reply[2] << 8) |
           (a->reply[3] << 16) | (a->reply[4] << 24);
    unsigned value2 = a->reply[5] | (a->reply[6] << 8) |
           (a->reply[7] << 16) | (a->reply[8] << 24);
//fprintf (stderr, "    %08x -> %08x %08x\n", addr, value, value2);
    value >>= 1;
    value |= value2 & 0x80000000;
    return value;
}

/*
 * Read a block of memory, multiple of 1 kbyte.
 */
static void pickit2_read_data (adapter_t *adapter,
    unsigned addr, unsigned nwords, unsigned *data)
{
    pickit2_adapter_t *a = (pickit2_adapter_t*) adapter;
    unsigned char buf [64];
    unsigned words_read;

    //fprintf (stderr, "PICkit2: read %d bytes from %08x\n", nwords*4, addr);
    if (! a->use_executable) {
        /* Without PE. */
        for (; nwords > 0; nwords--) {
            *data++ = pickit2_read_word (adapter, addr);
            addr += 4;
        }
        return;
    }

    /* Use PE to read memory. */
    for (words_read = 0; words_read < nwords; ) {
        /* Download addresses for 8 script runs. */
        unsigned i, k = 0;
        memset (buf, CMD_END_OF_BUFFER, 64);
        buf[k++] = CMD_CLEAR_DOWNLOAD_BUFFER;
        buf[k++] = CMD_DOWNLOAD_DATA;
        buf[k++] = 8 * 4;
        for (i = 0; i < 8; i++) {
            unsigned address = addr + words_read*4 + i*32*4;
            buf[k++] = address;
            buf[k++] = address >> 8;
            buf[k++] = address >> 16;
            buf[k++] = address >> 24;
        }
        pickit2_send_buf (a, buf, k);

        for (k = 0; k < 8; k++) {
            /* Read progmem. */
            pickit2_send (a, 17, CMD_CLEAR_UPLOAD_BUFFER,
                CMD_EXECUTE_SCRIPT, 13,
                    SCRIPT_JT2_SENDCMD, ETAP_FASTDATA,
                    SCRIPT_JT2_XFRFASTDAT_LIT,
                        0x20, 0, 1, 0,          // READ
                    SCRIPT_JT2_XFRFASTDAT_BUF,
                    SCRIPT_JT2_WAIT_PE_RESP,
                    SCRIPT_JT2_GET_PE_RESP,
                    SCRIPT_LOOP, 1, 31,
                CMD_UPLOAD_DATA_NOLEN);
            pickit2_recv (a);
            memcpy (data, a->reply, 64);
            data += 64/4;
            words_read += 64/4;

            /* Get second half of upload buffer. */
            pickit2_send (a, 1, CMD_UPLOAD_DATA_NOLEN);
            pickit2_recv (a);
            memcpy (data, a->reply, 64);
            data += 64/4;
            words_read += 64/4;
        }
    }
}

/*
 * Put data to download buffer.
 * Max 15 words (60 bytes).
 */
static void download_data (pickit2_adapter_t *a,
    unsigned *data, unsigned nwords, int clear_flag)
{
    unsigned char buf [64];
    unsigned i, k = 0;

    memset (buf, CMD_END_OF_BUFFER, 64);
    if (clear_flag)
        buf[k++] = CMD_CLEAR_DOWNLOAD_BUFFER;
    buf[k++] = CMD_DOWNLOAD_DATA;
    buf[k++] = nwords * 4;
    for (i=0; i<nwords; i++) {
        unsigned word = *data++;
        buf[k++] = word;
        buf[k++] = word >> 8;
        buf[k++] = word >> 16;
        buf[k++] = word >> 24;
    }
    pickit2_send_buf (a, buf, k);
}

/*
 * Write a word to flash memory.
 */
static void pickit2_program_word (adapter_t *adapter,
    unsigned addr, unsigned word)
{
    pickit2_adapter_t *a = (pickit2_adapter_t*) adapter;

    if (debug_level > 0)
        fprintf (stderr, "PICkit2: program word at %08x: %08x\n", addr, word);
    if (! a->use_executable) {
        /* Without PE. */
        fprintf (stderr, "PICkit2: slow flash write not implemented yet.\n");
        exit (-1);
    }
    /* Use PE to write flash memory. */
    pickit2_send (a, 22, CMD_CLEAR_UPLOAD_BUFFER,
        CMD_EXECUTE_SCRIPT, 18,
            SCRIPT_JT2_SENDCMD, ETAP_FASTDATA,
            SCRIPT_JT2_XFRFASTDAT_LIT,
                2, 0, 3, 0,                     // WORD_PROGRAM
            SCRIPT_JT2_XFRFASTDAT_LIT,
                (unsigned char) addr,
                (unsigned char) (addr >> 8),
                (unsigned char) (addr >> 16),
                (unsigned char) (addr >> 24),
            SCRIPT_JT2_XFRFASTDAT_LIT,
                (unsigned char) word,
                (unsigned char) (word >> 8),
                (unsigned char) (word >> 16),
                (unsigned char) (word >> 24),
            SCRIPT_JT2_GET_PE_RESP,
        CMD_UPLOAD_DATA);
    pickit2_recv (a);
    //fprintf (stderr, "PICkit2: word program PE response %u bytes: %02x...\n",
    //  a->reply[0], a->reply[1]);
    if (a->reply[0] != 4 || a->reply[1] != 0) { // response code 0 = success
        fprintf (stderr, "PICkit2: failed to program word %08x at %08x, reply = %02x-%02x-%02x-%02x-%02x\n",
            word, addr, a->reply[0], a->reply[1], a->reply[2], a->reply[3], a->reply[4]);
        exit (-1);
    }
}

/*
 * Flash write, 1-kbyte blocks.
 */
static void pickit2_program_block (adapter_t *adapter,
    unsigned addr, unsigned *data)
{
    pickit2_adapter_t *a = (pickit2_adapter_t*) adapter;
    unsigned nwords = 256;
    unsigned words_written;

    if (debug_level > 0)
        fprintf (stderr, "PICkit2: program %d bytes at %08x\n", nwords*4, addr);
    if (! a->use_executable) {
        /* Without PE. */
        fprintf (stderr, "PICkit2: slow flash write not implemented yet.\n");
        exit (-1);
    }
    /* Use PE to write flash memory. */
    unsigned nbytes = nwords * 4;
    pickit2_send (a, 20, CMD_CLEAR_UPLOAD_BUFFER,
        CMD_EXECUTE_SCRIPT, 17,
            SCRIPT_JT2_SENDCMD, ETAP_FASTDATA,
            SCRIPT_JT2_XFRFASTDAT_LIT,
                0, 0, 2, 0,                     // PROGRAM
            SCRIPT_JT2_XFRFASTDAT_LIT,
                (unsigned char) addr,
                (unsigned char) (addr >> 8),
                (unsigned char) (addr >> 16),
                (unsigned char) (addr >> 24),
            SCRIPT_JT2_XFRFASTDAT_LIT,
                (unsigned char) nbytes,
                (unsigned char) (nbytes >> 8),
                (unsigned char) (nbytes >> 16),
                (unsigned char) (nbytes >> 24));

    for (words_written = 0; words_written < nwords; ) {
        /* Download 256 bytes of data. */
        download_data (a, data, 15, 1);
        download_data (a, data+15, 15, 0);
        download_data (a, data+30, 15, 0);
        download_data (a, data+45, 15, 0);

        pickit2_send (a, 26,
            CMD_DOWNLOAD_DATA, 4*4,
                WORD_AS_BYTES (data[60]),
                WORD_AS_BYTES (data[61]),
                WORD_AS_BYTES (data[62]),
                WORD_AS_BYTES (data[63]),
            CMD_EXECUTE_SCRIPT, 6,              // execute
                SCRIPT_JT2_SENDCMD, ETAP_FASTDATA,
                SCRIPT_JT2_XFRFASTDAT_BUF,
                SCRIPT_LOOP, 1, 63);

        data += 256/4;
        words_written += 256/4;
    }
    //check_timeout (a, "PROGRAM");

    pickit2_send (a, 6, CMD_CLEAR_UPLOAD_BUFFER,
        CMD_EXECUTE_SCRIPT, 2,
            SCRIPT_JT2_PE_PROG_RESP,
            SCRIPT_JT2_GET_PE_RESP,
        CMD_UPLOAD_DATA);
    pickit2_recv (a);
    //fprintf (stderr, "PICkit2: program PE response %u bytes: %02x...\n",
    //  a->reply[0], a->reply[1]);
    if (a->reply[0] != 4 || a->reply[1] != 0) { // response code 0 = success
        fprintf (stderr, "PICkit2: failed to program flash memory at %08x, reply = %02x-%02x-%02x-%02x-%02x\n",
            addr, a->reply[0], a->reply[1], a->reply[2], a->reply[3], a->reply[4]);
        exit (-1);
    }
}

/*
 * Erase all flash memory.
 */
static void pickit2_erase_chip (adapter_t *adapter)
{
    pickit2_adapter_t *a = (pickit2_adapter_t*) adapter;

    //fprintf (stderr, "PICkit2: erase chip\n");
    pickit2_send (a, 11, CMD_CLEAR_UPLOAD_BUFFER, CMD_EXECUTE_SCRIPT, 8,
        SCRIPT_JT2_SENDCMD, TAP_SW_MTAP,
        SCRIPT_JT2_SENDCMD, MTAP_COMMAND,
        SCRIPT_JT2_XFERDATA8_LIT, MCHP_ERASE,
        SCRIPT_DELAY_LONG, 74);                 // 400 msec
    check_timeout (a, "chip erase");
}

/*
 * Initialize adapter PICkit2.
 * Return a pointer to a data structure, allocated dynamically.
 * When adapter not found, return 0.
 */
adapter_t *adapter_open_pickit2 (void)
{
    pickit2_adapter_t *a;
    struct usb_bus *bus;
    struct usb_device *dev;

    usb_init();
    usb_find_busses();
    usb_find_devices();
    for (bus = usb_get_busses(); bus; bus = bus->next) {
        for (dev = bus->devices; dev; dev = dev->next) {
            if (dev->descriptor.idVendor == MICROCHIP_VID &&
                (dev->descriptor.idProduct == PICKIT2_PID))
                goto found;
        }
    }
    /*fprintf (stderr, "USB adapter not found: vid=%04x, pid=%04x\n",
        MICROCHIP_VID, PICKIT2_PID);*/
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
        fprintf (stderr, "PICkit2: usb_open() failed\n");
        free (a);
        return 0;
    }
    if (usb_set_configuration (a->usbdev, 2) < 0) {
        fprintf (stderr, "PICkit2: cannot set USB configuration\n");
failed: usb_release_interface (a->usbdev, IFACE);
        usb_close (a->usbdev);
        free (a);
        return 0;
    }
    if (usb_claim_interface (a->usbdev, IFACE) < 0) {
        fprintf (stderr, "PICkit2: cannot claim USB interface\n");
        goto failed;
    }

    /* Read version of adapter. */
    pickit2_send (a, 2, CMD_CLEAR_UPLOAD_BUFFER, CMD_GET_VERSION);
    pickit2_recv (a);
    printf ("      Adapter: PICkit2 Version %d.%d.%d\n",
        a->reply[0], a->reply[1], a->reply[2]);

    /* Detach power from the board. */
    pickit2_send (a, 4, CMD_EXECUTE_SCRIPT, 2,
        SCRIPT_VDD_OFF,
        SCRIPT_VDD_GND_ON);

    /* Setup power voltage 3.3V, fault limit 2.81V. */
    unsigned vdd = (unsigned) (3.3 * 32 + 10.5) << 6;
    unsigned vdd_limit = (unsigned) ((2.81 / 5) * 255);
    pickit2_send (a, 4, CMD_SET_VDD, vdd, vdd >> 8, vdd_limit);

    /* Setup reset voltage 3.28V, fault limit 2.26V. */
    unsigned vpp = (unsigned) (3.28 * 18.61);
    unsigned vpp_limit = (unsigned) (2.26 * 18.61);
    pickit2_send (a, 4, CMD_SET_VPP, 0x40, vpp, vpp_limit);

    /* Setup serial speed. */
    unsigned divisor = 10;
    pickit2_send (a, 4, CMD_EXECUTE_SCRIPT, 2,
        SCRIPT_SET_ICSP_SPEED, divisor);

    /* Reset active low. */
    pickit2_send (a, 3, CMD_EXECUTE_SCRIPT, 1,
        SCRIPT_MCLR_GND_ON);

    /* Read board status. */
    pickit2_send (a, 2, CMD_CLEAR_UPLOAD_BUFFER, CMD_READ_STATUS);
    pickit2_recv (a);
    unsigned status = a->reply[0] | a->reply[1] << 8;
    if (debug_level > 0)
        fprintf (stderr, "PICkit2: status %04x\n", status);
    if ((status & ~STATUS_RESET) != (STATUS_VDD_GND_ON | STATUS_VPP_GND_ON)) {
        fprintf (stderr, "PICkit2: invalid status = %04x\n", status);
        goto failed;
    }

    /* Enable power to the board. */
    pickit2_send (a, 4, CMD_EXECUTE_SCRIPT, 2,
        SCRIPT_VDD_GND_OFF,
        SCRIPT_VDD_ON);

    /* Read board status. */
    pickit2_send (a, 2, CMD_CLEAR_UPLOAD_BUFFER, CMD_READ_STATUS);
    pickit2_recv (a);
    status = a->reply[0] | a->reply[1] << 8;
    if (debug_level > 0)
        fprintf (stderr, "PICkit2: status %04x\n", status);
    if (status != (STATUS_VDD_ON | STATUS_VPP_GND_ON)) {
        fprintf (stderr, "PICkit2: invalid status = %04x.\n", status);
        goto failed;
    }

    /* Enter programming mode. */
    pickit2_send (a, 42, CMD_CLEAR_UPLOAD_BUFFER, CMD_EXECUTE_SCRIPT, 39,
        SCRIPT_VPP_OFF,
        SCRIPT_MCLR_GND_ON,
        SCRIPT_VPP_PWM_ON,
        SCRIPT_BUSY_LED_ON,
        SCRIPT_SET_ICSP_PINS, 0,
        SCRIPT_DELAY_LONG, 20,                  // 100 msec
        SCRIPT_MCLR_GND_OFF,
        SCRIPT_VPP_ON,
        SCRIPT_DELAY_SHORT, 23,                 // 1 msec
        SCRIPT_VPP_OFF,
        SCRIPT_MCLR_GND_ON,
        SCRIPT_DELAY_SHORT, 47,                 // 2 msec
        SCRIPT_WRITE_BYTE_LITERAL, 0xb2,
        SCRIPT_WRITE_BYTE_LITERAL, 0xc2,
        SCRIPT_WRITE_BYTE_LITERAL, 0x12,
        SCRIPT_WRITE_BYTE_LITERAL, 0x0a,
        SCRIPT_MCLR_GND_OFF,
        SCRIPT_VPP_ON,
        SCRIPT_DELAY_LONG, 2,                   // 10 msec
        SCRIPT_SET_ICSP_PINS, 2,
        SCRIPT_JT2_SETMODE, 6, 0x1f,
        SCRIPT_JT2_SENDCMD, TAP_SW_MTAP,
        SCRIPT_JT2_SENDCMD, MTAP_COMMAND,
        SCRIPT_JT2_XFERDATA8_LIT, MCHP_STATUS);
    pickit2_send (a, 1, CMD_UPLOAD_DATA);
    pickit2_recv (a);
    if (debug_level > 1)
        fprintf (stderr, "PICkit2: got %02x-%02x\n", a->reply[0], a->reply[1]);
    if (a->reply[0] != 1) {
        fprintf (stderr, "PICkit2: cannot get MCHP STATUS\n");
        pickit2_finish (a, 0);
        goto failed;
    }
    if (! (a->reply[1] & MCHP_STATUS_CFGRDY)) {
        fprintf (stderr, "No device attached.\n");
        pickit2_finish (a, 0);
        goto failed;
    }
    if (! (a->reply[1] & MCHP_STATUS_CPS)) {
        fprintf (stderr, "Device is code protected and must be erased first.\n");
        pickit2_finish (a, 0);
        goto failed;
    }

    /* User functions. */
    a->adapter.close = pickit2_close;
    a->adapter.get_idcode = pickit2_get_idcode;
    a->adapter.load_executable = pickit2_load_executable;
    a->adapter.read_word = pickit2_read_word;
    a->adapter.read_data = pickit2_read_data;
    a->adapter.erase_chip = pickit2_erase_chip;
    a->adapter.program_block = pickit2_program_block;
    a->adapter.program_word = pickit2_program_word;
    return &a->adapter;
}
