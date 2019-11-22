/*
 * Interface to PIC32 ICSP port using Microchip PICkit2/PICkit3 USB adapter.
 *
 * To use PICkit3, you would need to upgrade the firmware
 * using free PICkit 3 Scripting Tool from Microchip.
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

#include "adapter.h"
#include "hidapi.h"
#include "pickit2.h"
#include "pic32.h"

typedef struct {
    /* Common part */
    adapter_t adapter;
    int is_pk3;
    const char *name;

    /* Device handle for libusb. */
    hid_device *hiddev;

    unsigned char reply [64];
    unsigned use_executive;
    unsigned serial_execution_mode;

} pickit_adapter_t;

/*
 * Programmable power voltages.
 */
#define VDD_VOLTAGE             3.3     /* Power supply */
#define VDD_LIMIT               2.81
#define VPP_VOLTAGE             3.28    /* Reset voltage */
#define VPP_LIMIT               2.26

/*
 * Identifiers of USB adapter.
 */
#define MICROCHIP_VID           0x04d8
#define PICKIT2_PID             0x0033  /* Microchip PICkit 2 */
#define PICKIT3_PID             0x900a  /* Microchip PICkit 3 */
#define ONBOARD_PID             0x8107  /* Onboard Programmer */
#define CHIPKIT_PID             0x8108  /* chipKIT Programmer */

/*
 * USB endpoints.
 */
#define OUT_EP                  0x01
#define IN_EP                   0x81

#define IFACE                   0
#define TIMO_MSEC               1000

#define WORD_AS_BYTES(w)  (unsigned char) (w), \
                          (unsigned char) ((w) >> 8), \
                          (unsigned char) ((w) >> 16), \
                          (unsigned char) ((w) >> 24)

static void pickit_send_buf(pickit_adapter_t *a, unsigned char *buf, unsigned nbytes)
{
    if (debug_level > 1) {
        int k;
        fprintf(stderr, "---Send");
        for (k=0; k<nbytes; ++k) {
            if (k != 0 && (k & 15) == 0)
                fprintf(stderr, "\n       ");
            fprintf(stderr, " %02x", buf[k]);
        }
        fprintf(stderr, "\n");
    }
    hid_write(a->hiddev, buf, 64);
}

static void pickit_send(pickit_adapter_t *a, unsigned argc, ...)
{
    va_list ap;
    unsigned i;
    unsigned char buf [64];

    memset(buf, CMD_END_OF_BUFFER, 64);
    va_start(ap, argc);
    for (i=0; i<argc; ++i)
        buf[i] = va_arg(ap, int);
    va_end(ap);
    pickit_send_buf(a, buf, i);
}

static void pickit_recv(pickit_adapter_t *a)
{
    if (hid_read(a->hiddev, a->reply, 64) != 64) {
        fprintf(stderr, "%s: error receiving packet\n", a->name);
        exit(-1);
    }
    if (debug_level > 1) {
        int k;
        fprintf(stderr, "--->>>>");
        for (k=0; k<64; ++k) {
            if (k != 0 && (k & 15) == 0)
                fprintf(stderr, "\n       ");
            fprintf(stderr, " %02x", a->reply[k]);
        }
        fprintf(stderr, "\n");
    }
}

static void check_timeout(pickit_adapter_t *a, const char *message)
{
    unsigned status;

    pickit_send(a, 1, CMD_READ_STATUS);
    pickit_recv(a);
    status = a->reply[0] | a->reply[1] << 8;
    if (status & STATUS_ICD_TIMEOUT) {
        fprintf(stderr, "%s: timed out at %s, status = %04x\n",
            a->name, message, status);
        exit(-1);
    }
}

/*
 * Put device to serial execution mode.
 */
static void serial_execution(pickit_adapter_t *a)
{
    if (a->serial_execution_mode)
        return;
    a->serial_execution_mode = 1;

    // Enter serial execution.
    if (debug_level > 0)
        fprintf(stderr, "%s: enter serial execution\n", a->name);
    pickit_send(a, 29, CMD_EXECUTE_SCRIPT, 27,
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

static void
step1_6_mm(pickit_adapter_t *a, unsigned nwords)
{

    if (debug_level > 0)
        fprintf(stderr, "%s: download PE loader\n", a->name);
    pickit_send(a, 20, CMD_CLEAR_DOWNLOAD_BUFFER,
        CMD_DOWNLOAD_DATA, 8,          //----------------- step 1
            WORD_AS_BYTES(0xa00041a4),
            WORD_AS_BYTES(0x02005084),
        CMD_EXECUTE_SCRIPT, 7,         //----------------- execute
            SCRIPT_JT2_SENDCMD, TAP_SW_ETAP,
            SCRIPT_JT2_SETMODE, 6, 0x1F,
            SCRIPT_JT2_XFERINST_BUF,
            SCRIPT_JT2_XFERINST_BUF);
    check_timeout(a, "step1");

    // Download the PE loader
    int i;
    for (i=0; i<PIC32_PEMM_LOADER_LEN; i+=2) {
        pickit_send(a, 20, CMD_CLEAR_DOWNLOAD_BUFFER,
            CMD_DOWNLOAD_DATA, 12,          //------------- step 5
                WORD_AS_BYTES((0x41A6
                    | pic32_pemm_loader[i] << 16)),
                WORD_AS_BYTES((0x50c6
                    | pic32_pemm_loader[i+1] << 16)),
                WORD_AS_BYTES(0x6e42eb40),
            CMD_EXECUTE_SCRIPT, 3,          //------------- execute
                SCRIPT_JT2_XFERINST_BUF,
                SCRIPT_JT2_XFERINST_BUF,
                SCRIPT_JT2_XFERINST_BUF);
        check_timeout(a, "step5");
    }

    // Jump to PE loader
    pickit_send(a, 47, CMD_CLEAR_DOWNLOAD_BUFFER,
        CMD_DOWNLOAD_DATA, 20,          //----------------- step 6
            WORD_AS_BYTES(0xa00041b9),
            WORD_AS_BYTES(0x02015339),
            WORD_AS_BYTES(0x0c004599),
            WORD_AS_BYTES(0x0c000c00),
            WORD_AS_BYTES(0x0c000c00),
        CMD_EXECUTE_SCRIPT, 22,         //----------------- execute
            SCRIPT_JT2_XFERINST_BUF,
            SCRIPT_JT2_XFERINST_BUF,
            SCRIPT_JT2_XFERINST_BUF,
            SCRIPT_JT2_XFERINST_BUF,
            SCRIPT_JT2_XFERINST_BUF,
            SCRIPT_JT2_SENDCMD, TAP_SW_ETAP,
            SCRIPT_JT2_SETMODE, 6, 0x1F,
            SCRIPT_JT2_SENDCMD, ETAP_FASTDATA,
            SCRIPT_JT2_XFRFASTDAT_LIT,
                0, 3, 0, 0xA0,                  // PE_ADDRESS = 0xA000_0300
            SCRIPT_JT2_XFRFASTDAT_LIT,
                (unsigned char) nwords,         // PE_SIZE
                (unsigned char) (nwords >> 8),
                0, 0);
    check_timeout(a, "step6");
}

static void
step1_6_mz(pickit_adapter_t *a, unsigned nwords)
{

    if (debug_level > 0)
        fprintf(stderr, "%s: download PE loader\n", a->name);
    pickit_send(a, 45, CMD_CLEAR_DOWNLOAD_BUFFER,
        CMD_DOWNLOAD_DATA, 28,          //----------------- step 1
            WORD_AS_BYTES(0x3c04bf88),  // lui a0, 0xbf88
            WORD_AS_BYTES(0x34842000),  // ori a0, 0x2000 - address of BMXCON
            WORD_AS_BYTES(0x3c05001f),  // lui a1, 0x1f
            WORD_AS_BYTES(0x34a50040),  // ori a1, 0x40   - a1 has 001f0040
            WORD_AS_BYTES(0xac850000),  // sw  a1, 0(a0)  - BMXCON initialized
                                        //----------------- step 2
            WORD_AS_BYTES(0x34050800),  // li  a1, 0x800  - a1 has 00000800
            WORD_AS_BYTES(0xac850010),  // sw  a1, 16(a0) - BMXDKPBA initialized
        CMD_EXECUTE_SCRIPT, 12,         //----------------- execute
            SCRIPT_JT2_SENDCMD, TAP_SW_ETAP,
            SCRIPT_JT2_SETMODE, 6, 0x1F,
            SCRIPT_JT2_XFERINST_BUF,
            SCRIPT_JT2_XFERINST_BUF,
            SCRIPT_JT2_XFERINST_BUF,
            SCRIPT_JT2_XFERINST_BUF,
            SCRIPT_JT2_XFERINST_BUF,
            SCRIPT_JT2_XFERINST_BUF,
            SCRIPT_JT2_XFERINST_BUF);
    check_timeout(a, "step1");

    pickit_send(a, 30, CMD_CLEAR_DOWNLOAD_BUFFER,
        CMD_DOWNLOAD_DATA, 20,          //----------------- step 3
            WORD_AS_BYTES(0x8c850040),  // lw  a1, 64(a0) - load BMXDMSZ
            WORD_AS_BYTES(0xac850020),  // sw  a1, 32(a0) - BMXDUDBA initialized
            WORD_AS_BYTES(0xac850030),  // sw  a1, 48(a0) - BMXDUPBA initialized
                                        //----------------- step 4
            WORD_AS_BYTES(0x3c04a000),  // lui a0, 0xa000
            WORD_AS_BYTES(0x34840800),  // ori a0, 0x800  - a0 has a0000800
        CMD_EXECUTE_SCRIPT, 5,          //----------------- execute
            SCRIPT_JT2_XFERINST_BUF,
            SCRIPT_JT2_XFERINST_BUF,
            SCRIPT_JT2_XFERINST_BUF,
            SCRIPT_JT2_XFERINST_BUF,
            SCRIPT_JT2_XFERINST_BUF);
    check_timeout(a, "step3");

    // Download the PE loader
    int i;
    for (i=0; i<PIC32_PE_LOADER_LEN; i+=2) {
        pickit_send(a, 25, CMD_CLEAR_DOWNLOAD_BUFFER,
            CMD_DOWNLOAD_DATA, 16,          //------------- step 5
                WORD_AS_BYTES((0x3c060000   // lui a2, PE_loader_hi++
                    | pic32_pe_loader[i])),
                WORD_AS_BYTES((0x34c60000   // ori a2, PE_loader_lo++
                    | pic32_pe_loader[i+1])),
                WORD_AS_BYTES(0xac860000),  // sw  a2, 0(a0)
                WORD_AS_BYTES(0x24840004),  // addiu a0, 4
            CMD_EXECUTE_SCRIPT, 4,          //------------- execute
                SCRIPT_JT2_XFERINST_BUF,
                SCRIPT_JT2_XFERINST_BUF,
                SCRIPT_JT2_XFERINST_BUF,
                SCRIPT_JT2_XFERINST_BUF);
        check_timeout(a, "step5");
    }

    // Jump to PE loader
    pickit_send(a, 42, CMD_CLEAR_DOWNLOAD_BUFFER,
        CMD_DOWNLOAD_DATA, 16,          //----------------- step 6
            WORD_AS_BYTES(0x3c19a000),  // lui t9, 0xa000
            WORD_AS_BYTES(0x37390800),  // ori t9, 0x800  - t9 has a0000800
            WORD_AS_BYTES(0x03200008),  // jr  t9
            WORD_AS_BYTES(0x00000000),  // nop
        CMD_EXECUTE_SCRIPT, 21,         //----------------- execute
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
                (unsigned char) nwords,         // PE_SIZE
                (unsigned char) (nwords >> 8),
                0, 0);
    check_timeout(a, "step6");
}

/*
 * Download programming executive (PE).
 */
static void pickit_load_executive(adapter_t *adapter,
    const unsigned *pe, unsigned nwords,
    unsigned pe_version)
{
    pickit_adapter_t *a = (pickit_adapter_t*) adapter;
    int i;

    a->name = a->adapter.family_name;
    //fprintf(stderr, "%s: load_executive\n", a->name);
    a->use_executive = 1;
    serial_execution(a);

	/* Round nwords to 10, to be compatible with routine below.
	 * This is why there are extra 0s in the executive file for padding. */
	nwords = (nwords%10) == 0 ? nwords : nwords + 10 - (nwords%10);

    if (a->adapter.family_name_short == FAMILY_MM)
        step1_6_mm(a, nwords);
    else
        step1_6_mz(a, nwords);

    // Download the PE itself (step 7-B)
    if (debug_level > 0)
        fprintf(stderr, "%s: download PE code\n", a->name);
    int nloops = (nwords + 9) / 10;
    for (i=0; i<nloops; i++, pe+=10) {          // download 10 words at a time
        pickit_send(a, 55, CMD_CLEAR_DOWNLOAD_BUFFER,
            CMD_DOWNLOAD_DATA, 40,
                WORD_AS_BYTES(pe[0]),
                WORD_AS_BYTES(pe[1]),
                WORD_AS_BYTES(pe[2]),
                WORD_AS_BYTES(pe[3]),
                WORD_AS_BYTES(pe[4]),
                WORD_AS_BYTES(pe[5]),
                WORD_AS_BYTES(pe[6]),
                WORD_AS_BYTES(pe[7]),
                WORD_AS_BYTES(pe[8]),
                WORD_AS_BYTES(pe[9]),
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
        check_timeout(a, "step7");
    }
    mdelay(100);

    // Download the PE instructions
    pickit_send(a, 15, CMD_CLEAR_DOWNLOAD_BUFFER,
        CMD_DOWNLOAD_DATA, 8,
            WORD_AS_BYTES(0x00000000),         // step 8 - jump to PE
            WORD_AS_BYTES(0xDEAD0000),
        CMD_EXECUTE_SCRIPT, 2,                  // execute
            SCRIPT_JT2_XFRFASTDAT_BUF,
            SCRIPT_JT2_XFRFASTDAT_BUF);
    check_timeout(a, "step8");
    mdelay(100);

    pickit_send(a, 11, CMD_CLEAR_UPLOAD_BUFFER,
        CMD_EXECUTE_SCRIPT, 8,
            SCRIPT_JT2_SENDCMD, ETAP_FASTDATA,
            SCRIPT_JT2_XFRFASTDAT_LIT,
                0x00, 0x00,                     // length = 0
                0x07, 0x00,                     // EXEC_VERSION
            SCRIPT_JT2_GET_PE_RESP);
    check_timeout(a, "EXEC_VERSION");
    pickit_send(a, 1, CMD_UPLOAD_DATA);
    pickit_recv(a);

    unsigned version = a->reply[3] | (a->reply[4] << 8);
    if (version != 0x0007) {                    // command echo
        fprintf(stderr, "%s: bad PE reply = %04x\n", a->name, version);
        exit(-1);
    }
    version = a->reply[1] | (a->reply[2] << 8);
    if (version != pe_version) {
        fprintf(stderr, "%s: bad PE version = %04x, expected %04x\n",
            a->name, version, pe_version);
        exit(-1);
    }
    if (debug_level > 0)
        fprintf(stderr, "%s: PE version = %04x\n", a->name, version);
}

#if 0
int pe_blank_check(pickit_adapter_t *a,
    unsigned int start, unsigned int nbytes)
{
    pickit_send(a, 21, CMD_CLEAR_UPLOAD_BUFFER, CMD_EXECUTE_SCRIPT, 18,
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
    check_timeout(a, "BLANK_CHECK");
    pickit_send(a, 1, CMD_UPLOAD_DATA);
    pickit_recv(a);
    if (a->reply[3] != 6 || a->reply[1] != 0) { // response code 0 = success
        return 0;
    }
    return 1;
}

int pe_get_crc(pickit_adapter_t *a,
    unsigned int start, unsigned int nbytes)
{
    pickit_send(a, 22, CMD_CLEAR_UPLOAD_BUFFER, CMD_EXECUTE_SCRIPT, 19,
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
    check_timeout(a, "GET_CRC");
    pickit_send(a, 1, CMD_UPLOAD_DATA);
    pickit_recv(a);
    if (a->reply[3] != 8 || a->reply[1] != 0) { // response code 0 = success
        return 0;
    }

    int crc = a->reply[5] | (a->reply[6] << 8);
    return crc;
}
#endif

static void pickit_finish(pickit_adapter_t *a, int power_on)
{
    /* Exit programming mode. */
    pickit_send(a, 18, CMD_CLEAR_UPLOAD_BUFFER, CMD_EXECUTE_SCRIPT, 15,
        SCRIPT_JT2_SETMODE, 5, 0x1f,
        SCRIPT_VPP_OFF,
        SCRIPT_MCLR_GND_ON,
        SCRIPT_VPP_PWM_OFF,
        SCRIPT_SET_ICSP_PINS, 6,                // set PGC high, PGD input
        SCRIPT_SET_ICSP_PINS, 2,                // set PGC low, PGD input
        SCRIPT_SET_ICSP_PINS, 3,                // set PGC and PGD as input
        SCRIPT_DELAY_LONG, 10,                  // 50 msec
        SCRIPT_BUSY_LED_OFF);

    if (! power_on) {
        /* Detach power from the board. */
        pickit_send(a, 4, CMD_EXECUTE_SCRIPT, 2,
            SCRIPT_VDD_OFF,
            SCRIPT_VDD_GND_ON);
    }

    /* Disable reset. */
    pickit_send(a, 3, CMD_EXECUTE_SCRIPT, 1,
        SCRIPT_MCLR_GND_OFF);

    /* Read board status. */
    check_timeout(a, "finish");
}

static void pickit_close(adapter_t *adapter, int power_on)
{
    pickit_adapter_t *a = (pickit_adapter_t*) adapter;
    //fprintf(stderr, "%s: close\n", a->name);

    pickit_finish(a, power_on);
    free(a);
}

/*
 * Read the Device Identification code
 */
static unsigned pickit_get_idcode(adapter_t *adapter)
{
    pickit_adapter_t *a = (pickit_adapter_t*) adapter;
    unsigned idcode;

    /* Read device id. */
    pickit_send(a, 12, CMD_CLEAR_UPLOAD_BUFFER, CMD_EXECUTE_SCRIPT, 9,
        SCRIPT_JT2_SENDCMD, TAP_SW_MTAP,
        SCRIPT_JT2_SENDCMD, MTAP_IDCODE,
        SCRIPT_JT2_XFERDATA32_LIT, 0, 0, 0, 0);
    pickit_send(a, 1, CMD_UPLOAD_DATA);
    pickit_recv(a);
    //fprintf(stderr, "%s: read id, %d bytes: %02x %02x %02x %02x\n", a->name,
    //  a->reply[0], a->reply[1], a->reply[2], a->reply[3], a->reply[4]);
    if (a->reply[0] != 4)
        return 0;
    idcode = a->reply[1] | a->reply[2] << 8 | a->reply[3] << 16 | a->reply[4] << 24;
    return idcode;
}

/*
 * Read a word from memory (without PE).
 */
static unsigned pickit_read_word(adapter_t *adapter, unsigned addr)
{
    pickit_adapter_t *a = (pickit_adapter_t*) adapter;
    unsigned times = (a->serial_execution_mode)?0:1;
    serial_execution(a);
		
	unsigned word1 = 0;
	unsigned word2 = 0;
	unsigned addr_lo = addr & 0xFFFF;
	unsigned addr_hi = (addr >> 16) & 0xFFFF;
	unsigned value = 0;
    do{
	    if (FAMILY_MX1 == a->adapter.family_name_short
		    || FAMILY_MX3 == a->adapter.family_name_short
		    || FAMILY_MK == a->adapter.family_name_short
		    || FAMILY_MZ == a->adapter.family_name_short
		    )
	    {

		    pickit_send(a, 49, CMD_CLEAR_DOWNLOAD_BUFFER,
		        CMD_CLEAR_UPLOAD_BUFFER,
		        CMD_DOWNLOAD_DATA, 24,
		            WORD_AS_BYTES(0x3c13ff20),              // lui s3, 0xFF20
		            WORD_AS_BYTES(0x3c080000 | addr_hi),    // lui t0, addr_hi
		            WORD_AS_BYTES(0x35080000 | addr_lo),    // ori t0, addr_lo
		            WORD_AS_BYTES(0x8d090000),              // lw t1, 0(t0)
		            WORD_AS_BYTES(0xae690000),              // sw t1, 0(s3)
		            WORD_AS_BYTES(0),                       // nop
		        CMD_EXECUTE_SCRIPT, 18,
		            SCRIPT_JT2_SENDCMD, TAP_SW_ETAP,
		            SCRIPT_JT2_SETMODE, 6, 0x1F,
		            SCRIPT_JT2_XFERINST_BUF,
		            SCRIPT_JT2_XFERINST_BUF,
		            SCRIPT_JT2_XFERINST_BUF,
		            SCRIPT_JT2_XFERINST_BUF,
		            SCRIPT_JT2_XFERINST_BUF,
		            SCRIPT_JT2_XFERINST_BUF,
		            SCRIPT_JT2_SENDCMD, ETAP_FASTDATA,      // read FastData
		            SCRIPT_JT2_XFERDATA32_LIT, 0, 0, 0, 0,
		        CMD_UPLOAD_DATA);
		    pickit_recv(a);
		    if (a->reply[0] != 4) {
		        fprintf(stderr, "%s: read word %08x: bad reply length=%u\n",
		            a->name, addr, a->reply[0]);
		        exit(-1);
		    }
		    word1 = a->reply[1] | (a->reply[2] << 8) |
		           (a->reply[3] << 16) | (a->reply[4] << 24);

		    pickit_send(a, 54, CMD_CLEAR_DOWNLOAD_BUFFER,
		        CMD_CLEAR_UPLOAD_BUFFER,
		        CMD_DOWNLOAD_DATA, 28,
		            WORD_AS_BYTES(0x3c13ff20),              // lui s3, 0xFF20
		            WORD_AS_BYTES(0x3c080000 | addr_hi),    // lui t0, addr_hi
		            WORD_AS_BYTES(0x35080000 | addr_lo),    // ori t0, addr_lo
		            WORD_AS_BYTES(0x8d090000),              // lw t1, 0(t0)
		            WORD_AS_BYTES(0x00094842),              // srl t1, 1
		            WORD_AS_BYTES(0xae690004),              // sw t1, 4(s3)
		            WORD_AS_BYTES(0),                       // nop
		        CMD_EXECUTE_SCRIPT, 19,
		            SCRIPT_JT2_SENDCMD, TAP_SW_ETAP,
		            SCRIPT_JT2_SETMODE, 6, 0x1F,
		            SCRIPT_JT2_XFERINST_BUF,
		            SCRIPT_JT2_XFERINST_BUF,
		            SCRIPT_JT2_XFERINST_BUF,
		            SCRIPT_JT2_XFERINST_BUF,
		            SCRIPT_JT2_XFERINST_BUF,
		            SCRIPT_JT2_XFERINST_BUF,
		            SCRIPT_JT2_XFERINST_BUF,
		            SCRIPT_JT2_SENDCMD, ETAP_FASTDATA,      // read FastData
		            SCRIPT_JT2_XFERDATA32_LIT, 0, 0, 0, 0,
		        CMD_UPLOAD_DATA);
		    pickit_recv(a);
	        word2 = a->reply[1] | (a->reply[2] << 8) |
               (a->reply[3] << 16) | (a->reply[4] << 24);
	        /* First word of reply contains 31 bits of value in MSB; LSB bit is garbage.
		     * Second word contains the MSB bit of the value. */
		    value = (word1 >> 1) | (word2 & 0x80000000);
	    }
	    else{
		    /* Else PIC32MM family */

            /* Like above, get 31 bits, and then the one MSB separately.
             * Issue could be how Pickit code transfers things - there is 1 CLK
             * per packet (fastdata?) missing compared to reference implementation */
		    pickit_send(a, 54, CMD_CLEAR_DOWNLOAD_BUFFER,
		        CMD_CLEAR_UPLOAD_BUFFER,
		        CMD_DOWNLOAD_DATA, 28,
		            WORD_AS_BYTES(0xFF2041B3),              // lui s3, FAST_DATA_REG(32:16). Set address of fastdata register
		            WORD_AS_BYTES(0x000041A8 | (addr_hi<<16)),    // lui t0, DATA_ADDRESS(31:16)
		            WORD_AS_BYTES(0x00005108 | (addr_lo<<16)),    // ori t0, DATA_ADDRESS(15:0)
		            WORD_AS_BYTES(0x0000FD28),              // lw t1, 0(t0) - read data
		            WORD_AS_BYTES(0x0000F933),              // sw t1, 0(s3) - store data to fast register
		            WORD_AS_BYTES(0x0c000c00),              // nop
		            WORD_AS_BYTES(0x0c000c00),              // nop
		        CMD_EXECUTE_SCRIPT, 19,
		            SCRIPT_JT2_SENDCMD, TAP_SW_ETAP,
		            SCRIPT_JT2_SETMODE, 6, 0x1F,
		            SCRIPT_JT2_XFERINST_BUF,
		            SCRIPT_JT2_XFERINST_BUF,
		            SCRIPT_JT2_XFERINST_BUF,
		            SCRIPT_JT2_XFERINST_BUF,
		            SCRIPT_JT2_XFERINST_BUF,
		            SCRIPT_JT2_XFERINST_BUF,
		            SCRIPT_JT2_XFERINST_BUF,
		            SCRIPT_JT2_SENDCMD, ETAP_FASTDATA,      // read FastData
		            SCRIPT_JT2_XFERDATA32_LIT, 0, 0, 0, 0,
		        CMD_UPLOAD_DATA);
		    pickit_recv(a);
		    if (a->reply[0] != 4) {
		        fprintf(stderr, "%s: read word %08x: bad reply length=%u\n",
		            a->name, addr, a->reply[0]);
		        exit(-1);
		    }
		    word1 = a->reply[1] | (a->reply[2] << 8) |
		           (a->reply[3] << 16) | (a->reply[4] << 24);

		    pickit_send(a, 59, CMD_CLEAR_DOWNLOAD_BUFFER,
		        CMD_CLEAR_UPLOAD_BUFFER,
		        CMD_DOWNLOAD_DATA, 32,
		            WORD_AS_BYTES(0xFF2041B3),              // lui s3, FAST_DATA_REG(32:16). Set address of fastdata register
		            WORD_AS_BYTES(0x000041A8 | (addr_hi<<16)),    // lui t0, DATA_ADDRESS(31:16)
		            WORD_AS_BYTES(0x00005108 | (addr_lo<<16)),    // ori t0, DATA_ADDRESS(15:0)
		            WORD_AS_BYTES(0x0000FD28),              // lw t1, 0(t0) - read data
		            WORD_AS_BYTES(0x08400129),              // srl t1, 1
		            WORD_AS_BYTES(0x0000F933),              // sw t1, 0(s3) - store data to fast register
		            WORD_AS_BYTES(0x0c000c00),              // nop
		            WORD_AS_BYTES(0x0c000c00),              // nop
		        CMD_EXECUTE_SCRIPT, 20,
		            SCRIPT_JT2_SENDCMD, TAP_SW_ETAP,
		            SCRIPT_JT2_SETMODE, 6, 0x1F,
		            SCRIPT_JT2_XFERINST_BUF,
		            SCRIPT_JT2_XFERINST_BUF,
		            SCRIPT_JT2_XFERINST_BUF,
		            SCRIPT_JT2_XFERINST_BUF,
		            SCRIPT_JT2_XFERINST_BUF,
		            SCRIPT_JT2_XFERINST_BUF,
		            SCRIPT_JT2_XFERINST_BUF,
		            SCRIPT_JT2_XFERINST_BUF,
		            SCRIPT_JT2_SENDCMD, ETAP_FASTDATA,      // read FastData
		            SCRIPT_JT2_XFERDATA32_LIT, 0, 0, 0, 0,
		        CMD_UPLOAD_DATA);
		    pickit_recv(a);
		    if (a->reply[0] != 4) {
		        fprintf(stderr, "%s: read word %08x: bad reply length=%u\n",
		            a->name, addr, a->reply[0]);
		        exit(-1);
		    }
		    word2 = a->reply[1] | (a->reply[2] << 8) |
		           (a->reply[3] << 16) | (a->reply[4] << 24);


           value = (word1 >> 1) | (word2 & 0x80000000);
	    }
    }while(times-- > 0);

    if (a->reply[0] != 4) {
        fprintf(stderr, "%s: read word %08x: bad reply length=%u\n",
            a->name, addr, a->reply[0]);
        exit(-1);
    }

    if (debug_level > 0)
        fprintf(stderr, "%s: %08x -> %08x\n", __func__, addr, value);
    return value;
}

/*
 * Read a block of memory, multiple of 1 kbyte.
 */
static void pickit_read_data(adapter_t *adapter,
    unsigned addr, unsigned nwords, unsigned *data)
{
    pickit_adapter_t *a = (pickit_adapter_t*) adapter;
    unsigned char buf [64];
    unsigned words_read;

//fprintf(stderr, "%s: read %d bytes from %08x\n", a->name, nwords*4, addr);
    if (! a->use_executive) {
        /* Without PE. */
        for (; nwords > 0; nwords--) {
            *data++ = pickit_read_word(adapter, addr);
            addr += 4;
        }
        return;
    }

    /* Use PE to read memory. */
    for (words_read = 0; words_read < nwords; ) {
        /* Download addresses for 8 script runs. */
        unsigned i, k = 0;
        memset(buf, CMD_END_OF_BUFFER, 64);
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
        pickit_send_buf(a, buf, k);

        for (k = 0; k < 8; k++) {
            /* Read progmem. */
            pickit_send(a, 17, CMD_CLEAR_UPLOAD_BUFFER,
                CMD_EXECUTE_SCRIPT, 13,
                    SCRIPT_JT2_SENDCMD, ETAP_FASTDATA,
                    SCRIPT_JT2_XFRFASTDAT_LIT,
                        0x20, 0, 1, 0,          // READ
                    SCRIPT_JT2_XFRFASTDAT_BUF,
                    SCRIPT_JT2_WAIT_PE_RESP,
                    SCRIPT_JT2_GET_PE_RESP,
                    SCRIPT_LOOP, 1, 31,
                CMD_UPLOAD_DATA_NOLEN);
            pickit_recv(a);
            memcpy(data, a->reply, 64);
//fprintf(stderr, "   ...%08x...\n", data[0]);
            data += 64/4;
            words_read += 64/4;

            /* Get second half of upload buffer. */
            pickit_send(a, 1, CMD_UPLOAD_DATA_NOLEN);
            pickit_recv(a);
            memcpy(data, a->reply, 64);
            data += 64/4;
            words_read += 64/4;
        }
    }
}

/*
 * Put data to download buffer.
 * Max 15 words (60 bytes).
 */
static void download_data(pickit_adapter_t *a,
    unsigned *data, unsigned nwords, int clear_flag)
{
    unsigned char buf [64];
    unsigned i, k = 0;

    memset(buf, CMD_END_OF_BUFFER, 64);
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
    pickit_send_buf(a, buf, k);
}

/*
 * Write a word to flash memory.
 */
static void pickit_program_word(adapter_t *adapter,
    unsigned addr, unsigned word)
{
    pickit_adapter_t *a = (pickit_adapter_t*) adapter;

    if (debug_level > 0)
        fprintf(stderr, "%s: program word at %08x: %08x\n", a->name, addr, word);
    if (! a->use_executive) {
        /* Without PE. */
        fprintf(stderr, "%s: slow flash write not implemented yet.\n", a->name);
        exit(-1);
    }
    /* Use PE to write flash memory. */
    pickit_send(a, 22, CMD_CLEAR_UPLOAD_BUFFER,
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
    pickit_recv(a);
    //fprintf(stderr, "%s: word program PE response %u bytes: %02x...\n",
    //  a->name, a->reply[0], a->reply[1]);
    if (a->reply[0] != 4 || a->reply[1] != 0) { // response code 0 = success
        fprintf(stderr, "%s: failed to program word %08x at %08x, reply = %02x-%02x-%02x-%02x-%02x\n",
            a->name, word, addr, a->reply[0], a->reply[1], a->reply[2], a->reply[3], a->reply[4]);
        exit(-1);
    }
}

/*
 * Write two words to flash memory.
 */
static void pickit_program_double_word(adapter_t *adapter,
    unsigned addr, unsigned word0, unsigned word1)
{
    pickit_adapter_t *a = (pickit_adapter_t*) adapter;

    if (debug_level > 0)
        fprintf(stderr, "%s: program words at %08x: %08x %08x\n", a->name, addr, word0, word1);
    if (! a->use_executive) {
        /* Without PE. */
        fprintf(stderr, "%s: slow flash write not implemented yet.\n", a->name);
        exit(-1);
    }
    /* Use PE to write flash memory. */
    pickit_send(a, 27, CMD_CLEAR_UPLOAD_BUFFER,
        CMD_EXECUTE_SCRIPT, 23,
            SCRIPT_JT2_SENDCMD, ETAP_FASTDATA,
            SCRIPT_JT2_XFRFASTDAT_LIT,
                0, 0, PE_DOUBLE_WORD_PGRM, 0,
            SCRIPT_JT2_XFRFASTDAT_LIT,
                (unsigned char) addr,
                (unsigned char) (addr >> 8),
                (unsigned char) (addr >> 16),
                (unsigned char) (addr >> 24),
            SCRIPT_JT2_XFRFASTDAT_LIT,
                (unsigned char) word0,
                (unsigned char) (word0 >> 8),
                (unsigned char) (word0 >> 16),
                (unsigned char) (word0 >> 24),
            SCRIPT_JT2_XFRFASTDAT_LIT,
                (unsigned char) word1,
                (unsigned char) (word1 >> 8),
                (unsigned char) (word1 >> 16),
                (unsigned char) (word1 >> 24),
           SCRIPT_JT2_GET_PE_RESP,
        CMD_UPLOAD_DATA);
    pickit_recv(a);
    //fprintf(stderr, "%s: word program PE response %u bytes: %02x...\n",
    //  a->name, a->reply[0], a->reply[1]);
    if (a->reply[0] != 4 || a->reply[1] != 0) { // response code 0 = success
        fprintf(stderr, "%s: failed to program words %08x %08x at %08x, reply = %02x-%02x-%02x-%02x-%02x\n",
            a->name, word0, word1, addr, a->reply[0], a->reply[1], a->reply[2], a->reply[3], a->reply[4]);
        exit(-1);
    }
}

/*
 * Write 4 words to flash memory.
 */
static void pickit_program_quad_word(adapter_t *adapter, unsigned addr,
    unsigned word0, unsigned word1, unsigned word2, unsigned word3)
{
    pickit_adapter_t *a = (pickit_adapter_t*) adapter;

    if (debug_level > 0)
        fprintf(stderr, "%s: program quad word at %08x: %08x-%08x-%08x-%08x\n",
            a->name, addr, word0, word1, word2, word3);
    if (! a->use_executive) {
        /* Without PE. */
        fprintf(stderr, "%s: slow flash write not implemented yet.\n", a->name);
        exit(-1);
    }

    /* Use PE to write flash memory. */
    pickit_send(a, 37, CMD_CLEAR_UPLOAD_BUFFER,
        CMD_EXECUTE_SCRIPT, 33,
            SCRIPT_JT2_SENDCMD, ETAP_FASTDATA,
            SCRIPT_JT2_XFRFASTDAT_LIT,
                0, 0, 13, 0,                    // QUAD_WORD_PROGRAM
            SCRIPT_JT2_XFRFASTDAT_LIT,
                (unsigned char) addr,
                (unsigned char) (addr >> 8),
                (unsigned char) (addr >> 16),
                (unsigned char) (addr >> 24),
            SCRIPT_JT2_XFRFASTDAT_LIT,
                (unsigned char) word0,
                (unsigned char) (word0 >> 8),
                (unsigned char) (word0 >> 16),
                (unsigned char) (word0 >> 24),
            SCRIPT_JT2_XFRFASTDAT_LIT,
                (unsigned char) word1,
                (unsigned char) (word1 >> 8),
                (unsigned char) (word1 >> 16),
                (unsigned char) (word1 >> 24),
            SCRIPT_JT2_XFRFASTDAT_LIT,
                (unsigned char) word2,
                (unsigned char) (word2 >> 8),
                (unsigned char) (word2 >> 16),
                (unsigned char) (word2 >> 24),
            SCRIPT_JT2_XFRFASTDAT_LIT,
                (unsigned char) word3,
                (unsigned char) (word3 >> 8),
                (unsigned char) (word3 >> 16),
                (unsigned char) (word3 >> 24),
            SCRIPT_JT2_GET_PE_RESP,
        CMD_UPLOAD_DATA);
    pickit_recv(a);
    //fprintf(stderr, "%s: word program PE response %u bytes: %02x...\n",
    //  a->name, a->reply[0], a->reply[1]);
    if (a->reply[0] != 4 || a->reply[1] != 0) { // response code 0 = success
        fprintf(stderr, "%s: failed to program quad word at %08x, reply = %02x-%02x-%02x-%02x-%02x\n",
            a->name, addr, a->reply[0], a->reply[1], a->reply[2], a->reply[3], a->reply[4]);
        exit(-1);
    }
}

/*
 * Flash write row of memory.
 */
static void pickit_program_row(adapter_t *adapter, unsigned addr,
    unsigned *data, unsigned words_per_row)
{
    pickit_adapter_t *a = (pickit_adapter_t*) adapter;
    unsigned i;

    if (debug_level > 0)
        fprintf(stderr, "%s: row program %u words at %08x\n",
            a->name, words_per_row, addr);
    if (! a->use_executive) {
        /* Without PE. */
        fprintf(stderr, "%s: slow flash write not implemented yet.\n", a->name);
        exit(-1);
    }
    /* Use PE to write flash memory. */

    pickit_send(a, 15, CMD_CLEAR_UPLOAD_BUFFER,
        CMD_EXECUTE_SCRIPT, 12,
            SCRIPT_JT2_SENDCMD, ETAP_FASTDATA,
            SCRIPT_JT2_XFRFASTDAT_LIT,
                words_per_row, 0, 0, 0,         // PROGRAM ROW
            SCRIPT_JT2_XFRFASTDAT_LIT,
                (unsigned char) addr,
                (unsigned char) (addr >> 8),
                (unsigned char) (addr >> 16),
                (unsigned char) (addr >> 24));

    /* Download data. */
    if (words_per_row == 32) {
        /* MX1/2 family. */
        download_data(a, data, 15, 1);
        download_data(a, data+15, 15, 0);

        pickit_send(a, 18,
            CMD_DOWNLOAD_DATA, 2*4,
                WORD_AS_BYTES(data[30]),
                WORD_AS_BYTES(data[31]),
            CMD_EXECUTE_SCRIPT, 6,              // execute
                SCRIPT_JT2_SENDCMD, ETAP_FASTDATA,
                SCRIPT_JT2_XFRFASTDAT_BUF,
                SCRIPT_LOOP, 1, 31);
    } else {
        /* MX3/4/5/6/7 or MZ family. */
        for (i = 0; i < words_per_row/64; i++) {
            /* Download 256 bytes of data. */
            download_data(a, data, 15, 1);
            download_data(a, data+15, 15, 0);
            download_data(a, data+30, 15, 0);
            download_data(a, data+45, 15, 0);

            pickit_send(a, 26,
                CMD_DOWNLOAD_DATA, 4*4,
                    WORD_AS_BYTES(data[60]),
                    WORD_AS_BYTES(data[61]),
                    WORD_AS_BYTES(data[62]),
                    WORD_AS_BYTES(data[63]),
                CMD_EXECUTE_SCRIPT, 6,              // execute
                    SCRIPT_JT2_SENDCMD, ETAP_FASTDATA,
                    SCRIPT_JT2_XFRFASTDAT_BUF,
                    SCRIPT_LOOP, 1, 63);

            data += 64;
        }
    }

    pickit_send(a, 5, CMD_CLEAR_UPLOAD_BUFFER,
        CMD_EXECUTE_SCRIPT, 1,
            SCRIPT_JT2_GET_PE_RESP,
        CMD_UPLOAD_DATA);

    pickit_recv(a);
    //fprintf(stderr, "%s: program PE response %u bytes: %02x...\n",
    //  a->name, a->reply[0], a->reply[1]);
    if (a->reply[0] != 4 || a->reply[1] != 0) { // response code 0 = success
        fprintf(stderr, "%s: failed to program row flash memory at %08x, reply = %02x-%02x-%02x-%02x-%02x\n",
            a->name, addr, a->reply[0], a->reply[1], a->reply[2], a->reply[3], a->reply[4]);
        exit(-1);
    }
}

/*
 * Erase all flash memory.
 */
static void pickit_erase_chip(adapter_t *adapter)
{
    pickit_adapter_t *a = (pickit_adapter_t*) adapter;

    //fprintf(stderr, "%s: erase chip\n", a->name);
    pickit_send(a, 11, CMD_CLEAR_UPLOAD_BUFFER, CMD_EXECUTE_SCRIPT, 8,
        SCRIPT_JT2_SENDCMD, TAP_SW_MTAP,
        SCRIPT_JT2_SENDCMD, MTAP_COMMAND,
        SCRIPT_JT2_XFERDATA8_LIT, MCHP_ERASE,
        SCRIPT_DELAY_LONG, 74);                 // 400 msec
    check_timeout(a, "chip erase");
}

/*
 * Initialize adapter PICkit2/PICkit3.
 * Return a pointer to a data structure, allocated dynamically.
 * When adapter not found, return 0.
 */
static adapter_t *open_pickit(hid_device *hiddev, int is_pk3)
{
    pickit_adapter_t *a;

    a = calloc(1, sizeof(*a));
    if (! a) {
        fprintf(stderr, "Out of memory\n");
        return 0;
    }
    a->hiddev = hiddev;
    a->is_pk3 = is_pk3;
    a->name = is_pk3 ? "PICkit3" : "PICkit2";

    /* Read version of adapter. */
    unsigned vers_major, vers_minor, vers_rev;
    if (a->is_pk3) {
        pickit_send(a, 2, CMD_GETVERSIONS_MPLAB, 0);
        pickit_recv(a);
        if (a->reply[30] != 'P' ||
            a->reply[31] != 'k' ||
            a->reply[32] != '3')
        {
            fprintf(stderr, "Reply %d%d%d\n", a->reply[30], a->reply[31], a->reply[32]);
            free(a);
            fprintf(stderr, "Incompatible PICkit3 firmware detected.\n");
            fprintf(stderr, "Please, upgrade the firmware using PICkit 3 Scripting Tool.\n");
            return 0;
        }
        vers_major = a->reply[33];
        vers_minor = a->reply[34];
        vers_rev = a->reply[35];
    } else {
        pickit_send(a, 2, CMD_CLEAR_UPLOAD_BUFFER, CMD_GET_VERSION);
        pickit_recv(a);
        vers_major = a->reply[0];
        vers_minor = a->reply[1];
        vers_rev = a->reply[2];
    }
    printf("      Adapter: %s Version %d.%d.%d\n",
        a->name, vers_major, vers_minor, vers_rev);

    /* Detach power from the board. */
    pickit_send(a, 4, CMD_EXECUTE_SCRIPT, 2,
        SCRIPT_VDD_OFF,
        SCRIPT_VDD_GND_ON);

    /* Setup power voltage 3.3V, fault limit 2.81V. */
    if (a->is_pk3) {
        /* PICkit 3 */
        unsigned vdd = (unsigned) (VDD_VOLTAGE * 8 + 2.5);
        pickit_send(a, 3, CMD_SET_VDD, vdd, vdd >> 8);
    } else {
        /* PICkit 2 */
        unsigned vdd = (unsigned) (VDD_VOLTAGE * 32 + 10.5) << 6;
        unsigned vdd_limit = (unsigned) ((VDD_LIMIT / 5) * 255);
        pickit_send(a, 4, CMD_SET_VDD, vdd, vdd >> 8, vdd_limit);
    }

    /* Setup reset voltage 3.28V, fault limit 2.26V. */
    if (a->is_pk3) {
        /* PICkit 3 */
        unsigned vpp = (unsigned) (VPP_VOLTAGE * 8 + 2.5);
        pickit_send(a, 3, CMD_SET_VPP, vpp, vpp >> 8);
    } else {
        /* PICkit 2 */
        unsigned vpp = (unsigned) (VPP_VOLTAGE * 18.61);
        unsigned vpp_limit = (unsigned) (VPP_LIMIT * 18.61);
        pickit_send(a, 4, CMD_SET_VPP, 0x40, vpp, vpp_limit);
    }

    /* Setup serial speed as 8MHz/divisor. */
    unsigned divisor = 10;
    pickit_send(a, 4, CMD_EXECUTE_SCRIPT, 2,
        SCRIPT_SET_ICSP_SPEED, divisor);

    /* Reset active low. */
    pickit_send(a, 3, CMD_EXECUTE_SCRIPT, 1,
        SCRIPT_MCLR_GND_ON);

    /* Read board status. */
    pickit_send(a, 2, CMD_CLEAR_UPLOAD_BUFFER, CMD_READ_STATUS);
    pickit_recv(a);
    unsigned status = a->reply[0] | a->reply[1] << 8;
    if (debug_level > 0)
        fprintf(stderr, "%s: status %04x\n", a->name, status);

    switch (status & ~(STATUS_RESET | STATUS_BUTTON_PRESSED)) {
    case STATUS_VPP_GND_ON:
    case STATUS_VPP_GND_ON | STATUS_VPP_ON:
        /* Explorer 16 board: no need to enable power. */
        break;

    case STATUS_VDD_GND_ON | STATUS_VDD_ON | STATUS_VPP_GND_ON:
    case STATUS_VDD_GND_ON | STATUS_VDD_ON | STATUS_VPP_GND_ON | STATUS_VPP_ON:
        /* Microstick II board: no need to enable power. */
        break;

    case STATUS_VDD_GND_ON | STATUS_VPP_GND_ON:
        /* Enable power to the board. */
        if (debug_level > 0)
            fprintf(stderr, "%s: enable power\n", a->name);
        pickit_send(a, 4, CMD_EXECUTE_SCRIPT, 2,
            SCRIPT_VDD_GND_OFF,
            SCRIPT_VDD_ON);

        /* Read board status. */
        pickit_send(a, 2, CMD_CLEAR_UPLOAD_BUFFER, CMD_READ_STATUS);
        pickit_recv(a);
        status = a->reply[0] | a->reply[1] << 8;
        if (debug_level > 0)
            fprintf(stderr, "%s: status %04x\n", a->name, status);
        if (status != (STATUS_VDD_ON | STATUS_VPP_GND_ON)) {
            fprintf(stderr, "%s: invalid status = %04x.\n", a->name, status);
            return 0;
        }
        /* Wait for power to stabilize. */
        mdelay(500);
        break;

    default:
        fprintf(stderr, "%s: invalid status = %04x\n", a->name, status);
        return 0;
    }

    /* Enter programming mode. */
    pickit_send(a, 42, CMD_CLEAR_UPLOAD_BUFFER, CMD_EXECUTE_SCRIPT, 39,
        SCRIPT_VPP_OFF,
        SCRIPT_MCLR_GND_ON,
        SCRIPT_VPP_PWM_ON,
        SCRIPT_BUSY_LED_ON,
        SCRIPT_SET_ICSP_PINS, 0,                // set PGC and PGD output low
        SCRIPT_DELAY_LONG, 20,                  // 100 msec
        SCRIPT_MCLR_GND_OFF,
        SCRIPT_VPP_ON,
        SCRIPT_DELAY_SHORT, 23,                 // 1 msec
        SCRIPT_VPP_OFF,
        SCRIPT_MCLR_GND_ON,
        SCRIPT_DELAY_SHORT, 47,                 // 2 msec
        SCRIPT_WRITE_BYTE_LITERAL, 0xb2,        // magic word
        SCRIPT_WRITE_BYTE_LITERAL, 0xc2,
        SCRIPT_WRITE_BYTE_LITERAL, 0x12,
        SCRIPT_WRITE_BYTE_LITERAL, 0x0a,
        SCRIPT_MCLR_GND_OFF,
        SCRIPT_VPP_ON,
        SCRIPT_DELAY_LONG, 2,                   // 10 msec
        SCRIPT_SET_ICSP_PINS, 2,                // set PGC low, PGD input
        SCRIPT_JT2_SETMODE, 6, 0x1f,
        SCRIPT_JT2_SENDCMD, TAP_SW_MTAP,
        SCRIPT_JT2_SENDCMD, MTAP_COMMAND,
        SCRIPT_JT2_XFERDATA8_LIT, MCHP_STATUS);
    pickit_send(a, 1, CMD_UPLOAD_DATA);
    pickit_recv(a);
    if (debug_level > 1)
        fprintf(stderr, "%s: got %02x-%02x\n", a->name, a->reply[0], a->reply[1]);
    if (a->reply[0] != 1) {
        fprintf(stderr, "%s: cannot get MCHP STATUS\n", a->name);
        pickit_finish(a, 0);
        return 0;
    }
    if (! (a->reply[1] & MCHP_STATUS_CFGRDY)) {
        fprintf(stderr, "No device attached.\n");
        pickit_finish(a, 0);
        return 0;
    }

    a->adapter.block_override = 0;
    a->adapter.flags = (AD_PROBE | AD_ERASE | AD_READ | AD_WRITE);

    if (! (a->reply[1] & MCHP_STATUS_CPS)) {
        fprintf(stderr, "%s: Device is code protected.\n", a->name);
        a->adapter.flags = (AD_ERASE);
    }

    /* User functions. */
    a->adapter.close = pickit_close;
    a->adapter.get_idcode = pickit_get_idcode;
    a->adapter.load_executive = pickit_load_executive;
    a->adapter.read_word = pickit_read_word;
    a->adapter.read_data = pickit_read_data;
    a->adapter.erase_chip = pickit_erase_chip;
    a->adapter.program_word = pickit_program_word;
    a->adapter.program_double_word = pickit_program_double_word;
    a->adapter.program_row = pickit_program_row;
    a->adapter.program_quad_word = pickit_program_quad_word;
    return &a->adapter;
}

/*
 * Initialize PICkit2 adapter.
 * Return a pointer to a data structure, allocated dynamically.
 * When adapter not found, return 0.
 */
adapter_t *adapter_open_pickit2(int vid, int pid, const char *serial)
{
    hid_device *hiddev;

    if (vid) {
        wchar_t buf[256];
        if (serial)
            mbstowcs(buf, serial, 256);
        hiddev = hid_open(vid, pid, serial ? buf : 0);
    } else {
        hiddev = hid_open(MICROCHIP_VID, PICKIT2_PID, 0);
    }
    if (! hiddev) {
        if (vid)
            fprintf(stderr, "PICkit2 not found: vid=%04x, pid=%04x, serial=%s\n",
                vid, pid, serial ? : "(none)");
        return 0;
    }
    return open_pickit(hiddev, 0);
}

/*
 * Initialize PICkit3 adapter.
 * Return a pointer to a data structure, allocated dynamically.
 * When adapter not found, return 0.
 */
adapter_t *adapter_open_pickit3(int vid, int pid, const char *serial)
{
    hid_device *hiddev;

    if (vid) {
        wchar_t buf[256];
        if (serial)
            mbstowcs(buf, serial, 256);
        hiddev = hid_open(vid, pid, serial ? buf : 0);
    } else {
        hiddev = hid_open(MICROCHIP_VID, PICKIT3_PID, 0);
        if (! hiddev)
            hiddev = hid_open(MICROCHIP_VID, CHIPKIT_PID, 0);
        if (! hiddev)
            hiddev = hid_open(MICROCHIP_VID, ONBOARD_PID, 0);
    }
    if (! hiddev) {
        if (vid)
            fprintf(stderr, "PICkit3 not found: vid=%04x, pid=%04x, serial=%s\n",
                vid, pid, serial ? : "(none)");
        return 0;
    }
    return open_pickit(hiddev, 1);
}
