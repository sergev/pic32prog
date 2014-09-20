/*
 * Interface to PIC32 JTAG port using bitbang adapter.
 *
 * Copyright (C) 2014 Serge Vakulenko
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
#include "pic32.h"

typedef struct {
    /* Common part */
    adapter_t adapter;
    const char *name;

    // TODO: add bitbang-specific data.
    int devfd;

    unsigned use_executive;
    unsigned serial_execution_mode;
} bitbang_adapter_t;

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
 * If there are any data in transmit buffer -
 * send them to device.
 */
static void bitbang_flush_output (bitbang_adapter_t *a)
{
    // TODO: Send transmit buffer to the device.
    // Get reply and fill the receive buffer.
}

static void bitbang_send (bitbang_adapter_t *a,
    unsigned tms_prolog_nbits, unsigned tms_prolog,
    unsigned tdi_nbits, unsigned long long tdi, int read_flag)
{
    // TODO: Fill the transmit buffer with output data.
}

static unsigned long long bitbang_recv (bitbang_adapter_t *a)
{
    unsigned long long word;

    /* Send a packet. */
    bitbang_flush_output (a);

    // TODO: Process a reply: one 64-bit word.
    word = 0xbadc0de;
    return word;
}

static void bitbang_reset (bitbang_adapter_t *a, int trst, int sysrst, int led)
{
    // TODO: Set values of /TRST, /SYSRTS and LED output signals.
}

static void bitbang_speed (bitbang_adapter_t *a, int khz)
{
    // TODO: Set the clock rate of bitbang adapter.
}

static void bitbang_close (adapter_t *adapter, int power_on)
{
    bitbang_adapter_t *a = (bitbang_adapter_t*) adapter;

    /* Clear EJTAGBOOT mode. */
    bitbang_send (a, 1, 1, 5, TAP_SW_ETAP, 0);    /* Send command. */
    bitbang_send (a, 6, 31, 0, 0, 0);             /* TMS 1-1-1-1-1-0 */
    bitbang_flush_output (a);

    /* Toggle /SYSRST. */
    bitbang_reset (a, 0, 1, 1);
    bitbang_reset (a, 0, 0, 0);

    // TODO: close the bitbang adapter.
    free (a);
}

/*
 * Read the Device Identification code
 */
static unsigned bitbang_get_idcode (adapter_t *adapter)
{
    bitbang_adapter_t *a = (bitbang_adapter_t*) adapter;
    unsigned idcode;

    /* Reset the JTAG TAP controller: TMS 1-1-1-1-1-0.
     * After reset, the IDCODE register is always selected.
     * Read out 32 bits of data. */
    bitbang_send (a, 6, 31, 32, 0, 1);
    idcode = bitbang_recv (a);
    return idcode;
}

/*
 * Put device to serial execution mode.
 */
static void serial_execution (bitbang_adapter_t *a)
{
    if (a->serial_execution_mode)
        return;
    a->serial_execution_mode = 1;

    /* Enter serial execution. */
    if (debug_level > 0)
        fprintf (stderr, "%s: enter serial execution\n", a->name);

    bitbang_send (a, 1, 1, 5, TAP_SW_ETAP, 0);    /* Send command. */
    bitbang_send (a, 1, 1, 5, ETAP_EJTAGBOOT, 0); /* Send command. */

    /* Check status. */
    bitbang_send (a, 1, 1, 5, TAP_SW_MTAP, 0);    /* Send command. */
    bitbang_send (a, 1, 1, 5, MTAP_COMMAND, 0);   /* Send command. */
    bitbang_send (a, 0, 0, 8, MCHP_DEASSERT_RST, 0);  /* Xfer data. */
    bitbang_send (a, 0, 0, 8, MCHP_FLASH_ENABLE, 0);  /* Xfer data. */
    bitbang_send (a, 0, 0, 8, MCHP_STATUS, 1);    /* Xfer data. */
    unsigned status = bitbang_recv (a);
    if (debug_level > 0)
        fprintf (stderr, "%s: status %04x\n", a->name, status);
    if ((status & ~MCHP_STATUS_DEVRST) !=
        (MCHP_STATUS_CPS | MCHP_STATUS_CFGRDY | MCHP_STATUS_FAEN)) {
        fprintf (stderr, "%s: invalid status = %04x (reset)\n", a->name, status);
        exit (-1);
    }

    /* Deactivate /SYSRST. */
    bitbang_reset (a, 0, 0, 1);
    mdelay (10);

    /* Check status. */
    bitbang_send (a, 1, 1, 5, TAP_SW_MTAP, 0);    /* Send command. */
    bitbang_send (a, 1, 1, 5, MTAP_COMMAND, 0);   /* Send command. */
    bitbang_send (a, 0, 0, 8, MCHP_STATUS, 1);    /* Xfer data. */
    status = bitbang_recv (a);
    if (debug_level > 0)
        fprintf (stderr, "%s: status %04x\n", a->name, status);
    if (status != (MCHP_STATUS_CPS | MCHP_STATUS_CFGRDY |
                   MCHP_STATUS_FAEN)) {
        fprintf (stderr, "%s: invalid status = %04x (no reset)\n", a->name, status);
        exit (-1);
    }

    /* Leave it in ETAP mode. */
    bitbang_send (a, 1, 1, 5, TAP_SW_ETAP, 0);    /* Send command. */
    bitbang_flush_output (a);
}

static void xfer_fastdata (bitbang_adapter_t *a, unsigned word)
{
    bitbang_send (a, 0, 0, 33, (unsigned long long) word << 1, 0);
}

static void xfer_instruction (bitbang_adapter_t *a, unsigned instruction)
{
    unsigned ctl;

    if (debug_level > 1)
        fprintf (stderr, "%s: xfer instruction %08x\n", a->name, instruction);

    // Select Control Register
    bitbang_send (a, 1, 1, 5, ETAP_CONTROL, 0);       /* Send command. */

    // Wait until CPU is ready
    // Check if Processor Access bit (bit 18) is set
    do {
        bitbang_send (a, 0, 0, 32, CONTROL_PRACC |    /* Xfer data. */
                                 CONTROL_PROBEN |
                                 CONTROL_PROBTRAP |
                                 CONTROL_EJTAGBRK, 1);
        ctl = bitbang_recv (a);
    } while (! (ctl & CONTROL_PRACC));

    // Select Data Register
    // Send the instruction
    bitbang_send (a, 1, 1, 5, ETAP_DATA, 0);          /* Send command. */
    bitbang_send (a, 0, 0, 32, instruction, 0);       /* Send data. */

    // Tell CPU to execute instruction
    bitbang_send (a, 1, 1, 5, ETAP_CONTROL, 0);       /* Send command. */
    bitbang_send (a, 0, 0, 32, CONTROL_PROBEN |       /* Send data. */
                             CONTROL_PROBTRAP, 0);
}

static unsigned get_pe_response (bitbang_adapter_t *a)
{
    unsigned ctl, response;

    // Select Control Register
    bitbang_send (a, 1, 1, 5, ETAP_CONTROL, 0);       /* Send command. */

    // Wait until CPU is ready
    // Check if Processor Access bit (bit 18) is set
    do {
        bitbang_send (a, 0, 0, 32, CONTROL_PRACC |    /* Xfer data. */
                                 CONTROL_PROBEN |
                                 CONTROL_PROBTRAP |
                                 CONTROL_EJTAGBRK, 1);
        ctl = bitbang_recv (a);
    } while (! (ctl & CONTROL_PRACC));

    // Select Data Register
    // Send the instruction
    bitbang_send (a, 1, 1, 5, ETAP_DATA, 0);          /* Send command. */
    bitbang_send (a, 0, 0, 32, 0, 1);                 /* Get data. */
    response = bitbang_recv (a);

    // Tell CPU to execute NOP instruction
    bitbang_send (a, 1, 1, 5, ETAP_CONTROL, 0);       /* Send command. */
    bitbang_send (a, 0, 0, 32, CONTROL_PROBEN |       /* Send data. */
                             CONTROL_PROBTRAP, 0);
    if (debug_level > 1)
        fprintf (stderr, "%s: get PE response %08x\n", a->name, response);
    return response;
}

/*
 * Read a word from memory (without PE).
 */
static unsigned bitbang_read_word (adapter_t *adapter, unsigned addr)
{
    bitbang_adapter_t *a = (bitbang_adapter_t*) adapter;
    unsigned addr_lo = addr & 0xFFFF;
    unsigned addr_hi = (addr >> 16) & 0xFFFF;

    serial_execution (a);

    //fprintf (stderr, "%s: read word from %08x\n", a->name, addr);
    xfer_instruction (a, 0x3c04bf80);           // lui s3, 0xFF20
    xfer_instruction (a, 0x3c080000 | addr_hi); // lui t0, addr_hi
    xfer_instruction (a, 0x35080000 | addr_lo); // ori t0, addr_lo
    xfer_instruction (a, 0x8d090000);           // lw t1, 0(t0)
    xfer_instruction (a, 0xae690000);           // sw t1, 0(s3)

    bitbang_send (a, 1, 1, 5, ETAP_FASTDATA, 0);  /* Send command. */
    bitbang_send (a, 0, 0, 33, 0, 1);             /* Get fastdata. */
    unsigned word = bitbang_recv (a) >> 1;

    if (debug_level > 0)
        fprintf (stderr, "%s: read word at %08x -> %08x\n", a->name, addr, word);
    return word;
}

/*
 * Read a memory block.
 */
static void bitbang_read_data (adapter_t *adapter,
    unsigned addr, unsigned nwords, unsigned *data)
{
    bitbang_adapter_t *a = (bitbang_adapter_t*) adapter;
    unsigned words_read, i;

    //fprintf (stderr, "%s: read %d bytes from %08x\n", a->name, nwords*4, addr);
    if (! a->use_executive) {
        /* Without PE. */
        for (; nwords > 0; nwords--) {
            *data++ = bitbang_read_word (adapter, addr);
            addr += 4;
        }
        return;
    }

    /* Use PE to read memory. */
    for (words_read = 0; words_read < nwords; words_read += 32) {

        bitbang_send (a, 1, 1, 5, ETAP_FASTDATA, 0);
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
static void bitbang_load_executive (adapter_t *adapter,
    const unsigned *pe, unsigned nwords, unsigned pe_version)
{
    bitbang_adapter_t *a = (bitbang_adapter_t*) adapter;

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
    bitbang_send (a, 1, 1, 5, TAP_SW_ETAP, 0);
    bitbang_send (a, 6, 31, 0, 0, 0);             /* TMS 1-1-1-1-1-0 */

    /* Send parameters for the loader (step 7-A).
     * PE_ADDRESS = 0xA000_0900,
     * PE_SIZE */
    bitbang_send (a, 1, 1, 5, ETAP_FASTDATA, 0);  /* Send command. */
    xfer_fastdata (a, 0xa0000900);
    xfer_fastdata (a, nwords);

    /* Download the PE itself (step 7-B). */
    if (debug_level > 0)
        fprintf (stderr, "%s: download PE\n", a->name);
    for (i=0; i<nwords; i++) {
        xfer_fastdata (a, *pe++);
    }
    bitbang_flush_output (a);
    mdelay (10);

    /* Download the PE instructions. */
    xfer_fastdata (a, 0);                       /* Step 8 - jump to PE. */
    xfer_fastdata (a, 0xDEAD0000);
    bitbang_flush_output (a);
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
static void bitbang_erase_chip (adapter_t *adapter)
{
    bitbang_adapter_t *a = (bitbang_adapter_t*) adapter;

    bitbang_send (a, 1, 1, 5, TAP_SW_MTAP, 0);    /* Send command. */
    bitbang_send (a, 1, 1, 5, MTAP_COMMAND, 0);   /* Send command. */
    bitbang_send (a, 0, 0, 8, MCHP_ERASE, 0);     /* Xfer data. */
    bitbang_flush_output (a);
    mdelay (400);

    /* Leave it in ETAP mode. */
    bitbang_send (a, 1, 1, 5, TAP_SW_ETAP, 0);    /* Send command. */
}

/*
 * Write a word to flash memory.
 */
static void bitbang_program_word (adapter_t *adapter,
    unsigned addr, unsigned word)
{
    bitbang_adapter_t *a = (bitbang_adapter_t*) adapter;

    if (debug_level > 0)
        fprintf (stderr, "%s: program word at %08x: %08x\n", a->name, addr, word);
    if (! a->use_executive) {
        /* Without PE. */
        fprintf (stderr, "%s: slow flash write not implemented yet.\n", a->name);
        exit (-1);
    }

    /* Use PE to write flash memory. */
    bitbang_send (a, 1, 1, 5, ETAP_FASTDATA, 0);  /* Send command. */
    xfer_fastdata (a, PE_WORD_PROGRAM << 16 | 2);
    bitbang_flush_output (a);
    xfer_fastdata (a, addr);                    /* Send address. */
    bitbang_flush_output (a);
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
static void bitbang_program_row (adapter_t *adapter, unsigned addr,
    unsigned *data, unsigned words_per_row)
{
    bitbang_adapter_t *a = (bitbang_adapter_t*) adapter;
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
    bitbang_send (a, 1, 1, 5, ETAP_FASTDATA, 0);  /* Send command. */
    xfer_fastdata (a, PE_ROW_PROGRAM << 16 | words_per_row);
    bitbang_flush_output (a);
    xfer_fastdata (a, addr);                    /* Send address. */

    /* Download data. */
    for (i = 0; i < words_per_row; i++) {
        if ((i & 7) == 0)
            bitbang_flush_output (a);
        xfer_fastdata (a, *data++);             /* Send word. */
    }
    bitbang_flush_output (a);

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
static void bitbang_verify_data (adapter_t *adapter,
    unsigned addr, unsigned nwords, unsigned *data)
{
    bitbang_adapter_t *a = (bitbang_adapter_t*) adapter;
    unsigned data_crc, flash_crc;

    //fprintf (stderr, "%s: verify %d words at %08x\n", a->name, nwords, addr);
    if (! a->use_executive) {
        /* Without PE. */
        fprintf (stderr, "%s: slow verify not implemented yet.\n", a->name);
        exit (-1);
    }

    /* Use PE to get CRC of flash memory. */
    bitbang_send (a, 1, 1, 5, ETAP_FASTDATA, 0);  /* Send command. */
    xfer_fastdata (a, PE_GET_CRC << 16);
    bitbang_flush_output (a);
    xfer_fastdata (a, addr);                    /* Send address. */
    bitbang_flush_output (a);
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
adapter_t *adapter_open_bitbang (const char *port)
{
    bitbang_adapter_t *a;

    a = calloc (1, sizeof (*a));
    if (! a) {
        fprintf (stderr, "adapter_open_bitbang: out of memory\n");
        return 0;
    }

    // TODO: find and open the connection to serial device `port'.
    a->devfd = 0;
    if (! a->devfd) {
        // TODO: fprintf (stderr, "%s: Bitbang adapter not found\n", port);
        free (a);
        return 0;
    }

    // TODO: Set default clock rate.
    int khz = 500;
    bitbang_speed (a, khz);

    /* Activate LED. */
    bitbang_reset (a, 0, 0, 1);

    /* Reset the JTAG TAP controller: TMS 1-1-1-1-1-0.
     * After reset, the IDCODE register is always selected.
     * Read out 32 bits of data. */
    unsigned idcode;
    bitbang_send (a, 6, 31, 32, 0, 1);
    idcode = bitbang_recv (a);
    if ((idcode & 0xfff) != 0x053) {
        /* Microchip vendor ID is expected. */
        if (debug_level > 0 || (idcode != 0 && idcode != 0xffffffff))
            fprintf (stderr, "%s: incompatible CPU detected, IDCODE=%08x\n",
                a->name, idcode);
        bitbang_reset (a, 0, 0, 0);
failed:
        // TODO: close (a->devfd);
        free (a);
        return 0;
    }

    /* Activate /SYSRST and LED. */
    bitbang_reset (a, 0, 1, 1);
    mdelay (10);

    /* Check status. */
    bitbang_send (a, 1, 1, 5, TAP_SW_MTAP, 0);    /* Send command. */
    bitbang_send (a, 1, 1, 5, MTAP_COMMAND, 0);   /* Send command. */
    bitbang_send (a, 0, 0, 8, MCHP_FLASH_ENABLE, 0);  /* Xfer data. */
    bitbang_send (a, 0, 0, 8, MCHP_STATUS, 1);    /* Xfer data. */
    unsigned status = bitbang_recv (a);
    if (debug_level > 0)
        fprintf (stderr, "%s: status %04x\n", a->name, status);
    if ((status & ~MCHP_STATUS_DEVRST) !=
        (MCHP_STATUS_CPS | MCHP_STATUS_CFGRDY | MCHP_STATUS_FAEN)) {
        fprintf (stderr, "%s: invalid status = %04x\n", a->name, status);
        bitbang_reset (a, 0, 0, 0);
        goto failed;
    }
    printf ("      Adapter: %s\n", a->name);

    /* User functions. */
    a->adapter.close = bitbang_close;
    a->adapter.get_idcode = bitbang_get_idcode;
    a->adapter.load_executive = bitbang_load_executive;
    a->adapter.read_word = bitbang_read_word;
    a->adapter.read_data = bitbang_read_data;
    a->adapter.verify_data = bitbang_verify_data;
    a->adapter.erase_chip = bitbang_erase_chip;
    a->adapter.program_word = bitbang_program_word;
    a->adapter.program_row = bitbang_program_row;
    return &a->adapter;
}
