// last edited: Sunday, 5 July, 2015 (release 2)

/*
 * Interface to PIC32 ICSP port using bitbang adapter.
 * Copyright (C) 2014 Serge Vakulenko
 *
 * Additions for talking to ascii ICSP programmer Copyright (C) 2015 Robert Rozee
 *
 * This file is part of PIC32PROG project, which is distributed
 * under the terms of the GNU General Public License (GPL).
 * See the accompanying file "COPYING" for more details.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include "adapter.h"
#include "pic32.h"
#include "serial.h"


typedef struct {
    adapter_t adapter;              /* Common part */

    int BitsToRead;                 // number of 'bits' waiting in Rx buffer
    int PendingHandshake;           // indicates a return handshake is expected

    unsigned TotalBitPairsSent;     // count of total # of TDI and TMS pairs sent
    unsigned TotalBitsReceived;     // count of total # of TDO bits recieved
    unsigned MaxBufferedWrites;     // max continuous characters written before read
    unsigned RunningWriteCount;     // running count of characters written, reset by read
    unsigned WriteCount;            // number of calls to serial_write
    unsigned Read1Count;            // number of calls to serial_read (data)
    unsigned Read2Count;            // number of calls to serial_read (handshakes)
    unsigned FDataCount;            // number of calls to xfer_fastdata
    unsigned DelayCount[4];         // number of calls to delay10mS (erase, xfer inst, PE resp, other)
    struct timeval T1, T2;          // record start and finishing timestamps

    unsigned use_executive;
    unsigned serial_execution_mode;
} bitbang_adapter_t;

static int DBG1 = 0;    // add format characters to command strings, print out
static int DBG2 = 0;    // print messages at entry to main routines
static int DBG3 = 0;    // print our row program parameters
static int CFG1 = 2;    // 1/2 configure for 64/1024 byte buffer in programming adapter
static int CFG2 = 1;    // 1/2 config to retrieve PrAcc and alert if (PrAcc != 1)
                        // (note: option 2 doubles programming time)

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
 * Sends a command ('8')to the programmer telling it to insert
 * a 10mS delay in the datastream being sent to the target. This
 * is the only reliable way to create a delay at the target.
 * (by RR)
 */
static void bitbang_delay10mS (bitbang_adapter_t *a, int caller)
{
    unsigned char ch;

    ch = '8';
    serial_write (&ch, 1);
    a->WriteCount++;
    a->DelayCount[caller]++;
}

/*
 * Current version of bitbang_send, sends a string of data out to the target encoded
 * as ASCII characters to be interpreted by an intellenent ICSP programmer.
 *
 * NOTE: this version of pic32prog implements the 2-wire communications mode via the
 * ICSP port. This means we need to encode TDI, TMS, and read_flag into one character,
 * sending all three out at once to an Arduino NANO acting as an intelligent programmer.
 * If read_flag is set, then the programmer needs to respond with a series of characters
 * indicating the values of TDO that the target responds with.
 *
 * Below is the currently implemented command set:
 *
 * 'd' : TDI = 0, TMS = 0, read_flag = 0   0x64
 * 'e' : TDI = 0, TMS = 1, read_flag = 0
 * 'f' : TDI = 1, TMS = 0, read_flag = 0
 * 'g' : TDI = 1, TMS = 1, read_flag = 0
 *
 * 'D' : TDI = 0, TMS = 0, read_flag = 1   0x44
 * 'E' : TDI = 0, TMS = 1, read_flag = 1
 * 'F' : TDI = 1, TMS = 0, read_flag = 1
 * 'G' : TDI = 1, TMS = 1, read_flag = 1
 *
 * '.' : no operation, used for formatting
 * '>' : request sync response - '<'
 *
 * '0' : clock out a 0 on PGD pin
 * '1' : clock out a 1 on PGD pin
 * '2' : set MCLR low
 * '3' : set MCLR high
 * '4' : turn target power off
 * '5' : turn target power on
 * '8' : insert 10mS delay
 * '?' : return ID string, "ascii JTAG XN"
 *
 * if the request is 'D'..'G', then respond with '0'/'1' to indicate TDO = 0/1
 *
 * (by RR)
 */
static void bitbang_send (bitbang_adapter_t *a,
    unsigned tms_nbits, unsigned tms,
    unsigned tdi_nbits, unsigned long long tdi, int read_flag)
{
    //
    // TMS is a command of 0 to 14 bits length, sent LSB first. TDI = 0 throughout.
    //
    // if nTDI<>0 then send TMS = 1-0-0 (TDI = 0).
    //
    // Next we send out the TDI bits (up to 64, LSB first), with TMS=0 for n-1 bits;
    // the last TDI bit should be accompanied with TMS = 1.
    //
    // if nTDI<>0 then send TMS = 1-0 (TDI = 0).
    //
    // NOTE: if read_flag == 1, then read TDO on last TMS bit and each of n-1 TDI bits.
    //       if read_flag == 2, only read TDO on the last TMS bit (to just get PrAcc)
    //

    unsigned char buffer[110];  // @@@@@@@@@@ BUFFERED WRITES VERSION @@@@@@@@@@
    int index = 0;              // index of next slot to use in buffer
    int count = 0;              // count of number of TDI/TMS pairs
    int i, n;
    unsigned char ch;

    if (read_flag && (a->BitsToRead != 0))
        fprintf (stderr, "WARNING - double read request (in send)\n");
    if (read_flag && (tdi_nbits == 0))
        fprintf (stderr, "WARNING - request to read 0 bits (in send)\n");

    for (i = tms_nbits; i > 0; i--) {           // for each of the n bits...
        ch = (tms & 1) + 'd';                   // d, e, f, g
        buffer [index++] = ch;                  // append to buffer
        tms >>= 1;                              // shift TMS right one bit
    }
    count += tms_nbits;

    if (DBG1 && (tms_nbits != 0))
        buffer[index++] = '.';                  // spacer, ignored by programmer

    if (tdi_nbits != 0) {                       // 1-0-0 if nTDI <> 0
        ch = 1 + 'd';
        buffer[index++] = ch;
        ch = 0 + 'd';
        buffer[index++] = ch;
        ch = 0 + (read_flag ? 'D' : 'd');
        buffer[index++] = ch;
        count += 3;
        if (DBG1)
            buffer[index++] = '.';              // spacer, ignored by programmer
    }

    for (i = tdi_nbits; i > 0; i--) {
        ch = ((tdi & 1) << 1) + (i == 1) +      // TMS=0 for n-1 bits, then 1 on last bit
             ((read_flag == 1 && i != 1) ?      // 0 = no read, 1 = normal read, 2 = oPrAcc read
              'D' : 'd');                       // UC = read, LC = none, no read on last bit
        buffer[index++] = ch;                   // append to buffer
        tdi >>= 1;                              // shift TDI right one bit
    }
    count += tdi_nbits;

    if (tdi_nbits != 0) {                       // 1-0 if nTDI <> 0
        if (DBG1)
            buffer[index++] = '.';              // spacer, ignored by programmer
        ch = 1 + 'd';
        buffer[index++] = ch;
        ch = 0 + 'd';
        buffer[index++] = ch;
        count += 2;
    }

    //
    // Control handshaking for ICSP programmers
    //

    if (a->PendingHandshake)
    {
        //////// this code is also duplicated in bitbang_send ////////
        if (a->RunningWriteCount > a->MaxBufferedWrites)
            a->MaxBufferedWrites = a->RunningWriteCount;
        a->RunningWriteCount = 0;
        //////////////////////////////////////////////////////////////

        a->PendingHandshake = 0;

        n = serial_read (&ch, 1);
        a->Read2Count++;

        if (n != 1 || ch != '<')
            fprintf (stderr, "WARNING - handshake read error (in send)\n");
    }

    //
    // the below block is to implement handshake on
    // EVERY write that does not have (read_flag != 0)
    //
    if (CFG1 == 1 && !read_flag)
    {
        buffer[index++] = '>';
        a->PendingHandshake = 1;
    }

    //
    // this block is to implement handshake on every 900 bytes sent out
    // NOTE: assumes the Rx buffer in the programmer is 1024 bytes long
    //                                                  **********

    if (CFG1 == 2 && !read_flag && (a->RunningWriteCount + index) > 900)    // 900 + 50 < 1024
    {
        buffer[index++] = '>';
        a->PendingHandshake = 1;
    }

    //
    // end of handshaking code
    //

    buffer[index] = 0;          // append trailing zero so can print as a string
    if (DBG1)
        fprintf (stderr, "n=%i, <%s> read=%i\n", index, buffer, read_flag);

    a->TotalBitPairsSent += count;
    a->RunningWriteCount += index;

    if (a->BitsToRead != 0)
        fprintf (stderr, "WARNING - write while pending read (in send)\n");

    serial_write (buffer, index);
    a->WriteCount++;

    if (read_flag)
        a->BitsToRead += tdi_nbits;
}

/*
 * (by RR)
 */
static unsigned long long bitbang_recv (bitbang_adapter_t *a)
{
    unsigned char buffer[70];
    unsigned long long word;
    int n, i;

    //////// this code is also duplicated in bitbang_send ////////
    if (a->RunningWriteCount > a->MaxBufferedWrites)
        a->MaxBufferedWrites = a->RunningWriteCount;
    a->RunningWriteCount = 0;
    //////////////////////////////////////////////////////////////

    if (a->PendingHandshake)
        fprintf (stderr, "WARNING - handshake pending error (in recv)\n");

    n = serial_read (buffer, a->BitsToRead);
    a->Read1Count++;

    if (n != a->BitsToRead)
        fprintf (stderr,
            "WARNING - fewer bits read (%i) than expected (%i) (in recv)\n",
                                        n,          a->BitsToRead);

    word = 0;

    for (i = 0; i < n; i++) {
        if (buffer[i] == '1')
            word |= 1 << i;
        else if (buffer[i] != '0')
            fprintf (stderr,
                "WARNING - unexpected character (0x%02x) returned (in recv)\n",
                                               buffer[i]);
    }

    if (DBG1) {
        unsigned L4 = word >> 48;
        unsigned L3 = (word >> 32) & 0xFFFF;
        unsigned L2 = (word >> 16) & 0xFFFF;
        unsigned L1 = word & 0xFFFF;
        fprintf (stderr, "TDO = %04x %04x %04x %04x (%i bits)\n",
                                 L4,  L3,  L2,  L1, a->BitsToRead);
    }

    a->TotalBitsReceived += a->BitsToRead;
    a->BitsToRead = 0;
    return word;
}

/*
 * this routine performs the functions:
 * (1) power up the target, then send out the ICSP signature to enable ICSP programming mode;
 * (0) power down the target in an orderly fashion.
 * (by RR)
 */
static void bitbang_ICSP_enable (bitbang_adapter_t *a, int ICSP_EN)
{
    if (ICSP_EN)
    {
        // 50mS delay after powerup, pulse MCLR high, send signature, set MCLR high, 10mS delay
        unsigned char buffer[64] = "5.88888.32.8.0100.1101.0100.0011.0100.1000.0101.0000.8.3.8......";
                                 // 0000000001111111111222222222233333333334444444444555555555566666
                                 // 1234567890123456789012345678901234567890123456789012345678901234

        serial_write (buffer, 64);
        usleep (150000);    // 150mS delay to allow the above to percolate through the system
    }
    else
    {
        // 50mS delay then cut power
        unsigned char buffer[16] = "88888.4.........";
                                 // 0000000001111111
                                 // 1234567890123456

        serial_write (buffer, 16);

        // 100mS delay to allow the above to percolate through the system
        usleep (100000);
    }
}

static void bitbang_close (adapter_t *adapter, int power_on)
{
    bitbang_adapter_t *a = (bitbang_adapter_t*) adapter;

    usleep (100000);

    /* Clear EJTAGBOOT mode. */
    bitbang_send (a, 1, 1, 5, TAP_SW_ETAP, 0);    /* Send command. */
    bitbang_send (a, 6, 31, 0, 0, 0);             /* TMS 1-1-1-1-1-0 */
    // (force the Chip TAP controller into Run Test/Idle state)

    //
    // not entirely sure the above sequence is correct.
    // see pg 29 of microchip programming docs
    //

    bitbang_ICSP_enable (a, 0);
    gettimeofday (&a->T2, 0);

    printf ("\n");
    printf ("total TDI/TMS pairs sent = %i pairs\n", a->TotalBitPairsSent);
    printf ("total TDO bits received  = %i bits\n",  a->TotalBitsReceived);
    printf ("maximum continuous write = %i chars\n", a->MaxBufferedWrites);

    printf ("O/S serial writes        = %i\n", a->WriteCount);
    printf ("O/S serial reads (data)  = %i\n", a->Read1Count);
    printf ("O/S serial reads (sync)  = %i\n", a->Read2Count);
    printf ("XferFastData count       = %i\n", a->FDataCount);
    printf ("10mS delays (E/X/R)      = %i/%i/%i\n", a->DelayCount[0],
                                                     a->DelayCount[1],
                                                     a->DelayCount[2]);
    printf ("elapsed programming time = %lum %02lus\n", (a->T2.tv_sec - a->T1.tv_sec) / 60,
                                                        (a->T2.tv_sec - a->T1.tv_sec) % 60);

    serial_close();                    // at this point we are exiting application???
//  free (a);                          // suspect this line was causing XP CRASHES
                                       // - shouldn't be needed anyway
}

/*
 * Read the Device Identification code
 */
static unsigned bitbang_get_idcode (adapter_t *adapter)
{
    bitbang_adapter_t *a = (bitbang_adapter_t*) adapter;
    unsigned idcode;

    if (DBG2)
        fprintf (stderr, "get_idcode\n");

    /* Reset the JTAG TAP controller: TMS 1-1-1-1-1-0.
     * After reset, the IDCODE register is always selected.
     * Read out 32 bits of data. */
    bitbang_send (a, 6, 31, 32, 0, 1);
    idcode = bitbang_recv (a);
    return idcode;
}

/*
 * Put device in serial execution mode. This is an alternative version
 * taken directly from the microchip application note. The original
 * version threw up a status error and then did an exit(-1), but
 * when the exit was commented out still seemed to function.
 * SEE: "PIC32 Flash Programming Specification 60001145N.pdf"
 * (by RR)
 */
static void serial_execution (bitbang_adapter_t *a)
{
    if (DBG2)
        fprintf (stderr, "serial_execution\n");

    if (a->serial_execution_mode)
        return;
    a->serial_execution_mode = 1;

    /* Enter serial execution. */
    if (debug_level > 0)
        fprintf (stderr, "enter serial execution\n");

    bitbang_send (a, 1, 1, 5, TAP_SW_MTAP, 0);    /* Send command. */             // 1.
    bitbang_send (a, 1, 1, 5, MTAP_COMMAND, 0);   /* Send command. */             // 2.
    bitbang_send (a, 0, 0, 8, MCHP_STATUS, 1);    /* Xfer data. */                // 3.
    unsigned status = bitbang_recv (a);
    if (debug_level > 0)
        fprintf (stderr, "status %04x\n", status);
    if ((status & MCHP_STATUS_CPS) == 0) {
        fprintf (stderr, "invalid status = %04x (code protection)\n", status);    // 4.
        exit (-1);
    }

    bitbang_send (a, 0, 0, 8, MCHP_ASSERT_RST, 0);  /* Xfer data. */              // 5.

    bitbang_send (a, 1, 1, 5, TAP_SW_ETAP, 0);    /* Send command. */             // 6.
    bitbang_send (a, 1, 1, 5, ETAP_EJTAGBOOT, 0); /* Send command. */             // 7.


    bitbang_send (a, 1, 1, 5, TAP_SW_MTAP, 0);    /* Send command. */             // 8.
    bitbang_send (a, 1, 1, 5, MTAP_COMMAND, 0);   /* Send command. */             // 9.
    bitbang_send (a, 0, 0, 8, MCHP_DEASSERT_RST, 0);  /* Xfer data. */            // 10.

    if (memcmp(a->adapter.family_name, "mz", 2) != 0)     // not needed for MZ processors
        bitbang_send (a, 0, 0, 8, MCHP_FLASH_ENABLE, 0);  /* Xfer data. */        // 11.

    bitbang_send (a, 1, 1, 5, TAP_SW_ETAP, 0);    /* Send command. */             // 12.
}

//
// Shouldn't XferFastData check the value of PrAcc returned in
// the LSB? We don't seem to be even reading this back.
// UPDATE1: The check is not needed in 4-wire JTAG mode, seems
// to only be required if in 2-wire ICSP mode. It would also
// slow us down horribly.
// UPDATE2: We are operating at such a slow speed that the PrAcc
// check is not really needed. To date, have never seen PrAcc != 1
//
static void xfer_fastdata (bitbang_adapter_t *a, unsigned word)
{
    a->FDataCount++;

    if (CFG2 == 1)
        bitbang_send (a, 0, 0, 33, (unsigned long long) word << 1, 0);

    if (CFG2 == 2) {
        bitbang_send (a, 0, 0, 33, (unsigned long long) word << 1, 2);
        unsigned status = bitbang_recv (a);
        if (! (status & 1))
            printf ("!");
    }
    //
    // could add in code above to handle retrying if PrAcc == 0
    //
    // a better (faster) approach may be to 'accumulate' PrAcc at the
    // programming adaptor, and then check the value at the end of a series
    // of xfer_fastdata calls. we would need to implement an extra ascii
    // command to use instead of 'D' (0x44) in the TDI header; '+' could be
    // used as the check command (PrAcc |= TDO), while '=' used to read out
    // the result (PrAcc) and reset the accumulator (PrAcc = 1) at the
    // programming adaptor.
}

static void xfer_instruction (bitbang_adapter_t *a, unsigned instruction)
{
    unsigned ctl;

    if (debug_level > 1)
        fprintf (stderr, "xfer instruction %08x\n", instruction);

    // Select Control Register
    bitbang_send (a, 1, 1, 5, ETAP_CONTROL, 0);       /* Send command. */

    // Wait until CPU is ready
    // Check if Processor Access bit (bit 18) is set

    int i = 0;
    do {
        if (i > 100)
            bitbang_delay10mS (a, 1);
        bitbang_send (a, 0, 0, 32, CONTROL_PRACC |    /* Xfer data. */
                                  CONTROL_PROBEN |
                                CONTROL_PROBTRAP |
                             /* CONTROL_EJTAGBRK */ 0, 1);  // this bit should NOT be set

        // Microchip document 60001145N, "PIC32 Flash Programming
        // Specification, DS60001145N page 18 stipulates 0x0004C000:
        // CONTROL_PRACC | CONTROL_PROBEN | CONTROL_PROBTRAP

        ctl = bitbang_recv (a);
        i++;
    } while (! (ctl & CONTROL_PRACC) && i < 150);

    if (i == 150) {
        fprintf (stderr, "PE response, PrAcc not set (in XferInstruction)\n");
        exit (-1);
    }

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

    int i = 0;
    do {
        if (i > 100)
            bitbang_delay10mS (a, 2);
        bitbang_send (a, 0, 0, 32, CONTROL_PRACC |    /* Xfer data. */
                                  CONTROL_PROBEN |
                                CONTROL_PROBTRAP |
                             /* CONTROL_EJTAGBRK */ 0, 1);  // this bit should NOT be set

        // Microchip document 60001145N, "PIC32 Flash Programming
        // Specification, DS60001145N page 30 stipulates 0x0004C000:
        // CONTROL_PRACC | CONTROL_PROBEN | CONTROL_PROBTRAP

        ctl = bitbang_recv (a);
        i++;
    } while (! (ctl & CONTROL_PRACC) && i < 150);

    if (i == 150) {
        fprintf (stderr, "PE response, PrAcc not set (in GetPEResponse)\n");
        exit (-1);
    }

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
        fprintf (stderr, "get PE response %08x\n", response);
    return response;
}

/*
 * Read a word from memory (without PE).
 *
 * Only used to read configuration words.
 */
static unsigned bitbang_read_word (adapter_t *adapter, unsigned addr)
{
    bitbang_adapter_t *a = (bitbang_adapter_t*) adapter;
    unsigned addr_lo = addr & 0xFFFF;
    unsigned addr_hi = (addr >> 16) & 0xFFFF;

    if (DBG2)
        fprintf (stderr, "read_word\n");

    serial_execution (a);

    xfer_instruction (a, 0x3c04bf80);           // lui s3, 0xFF20
    xfer_instruction (a, 0x3c080000 | addr_hi); // lui t0, addr_hi
    xfer_instruction (a, 0x35080000 | addr_lo); // ori t0, addr_lo
    xfer_instruction (a, 0x8d090000);           // lw t1, 0(t0)
    xfer_instruction (a, 0xae690000);           // sw t1, 0(s3)

    bitbang_send (a, 1, 1, 5, ETAP_FASTDATA, 0);  /* Send command. */
    bitbang_send (a, 0, 0, 33, 0, 1);             /* Get fastdata. */
    unsigned word = bitbang_recv (a) >> 1;

    if (debug_level > 0)
        fprintf (stderr, "read word at %08x -> %08x\n", addr, word);
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

    if (DBG2)
        fprintf (stderr, "read_data\n");

    if (! a->use_executive) {
        /* Without PE. */
        for (i = nwords; i > 0; i--) {
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
            fprintf (stderr, "bad READ response = %08x, expected %08x\n",
                                                response,     PE_READ << 16);
            exit (-1);
        }
        for (i = 0; i < 32; i++) {
            *data++ = get_pe_response (a);          /* Get data */
        }
        addr += 32 * 4;
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

    printf ("   Loading PE: ");

    if (memcmp(a->adapter.family_name, "mz", 2) != 0) {            // steps 1. to 3. not needed for MZ processors
        /* Step 1. */
        xfer_instruction (a, 0x3c04bf88);   // lui a0, 0xbf88
        xfer_instruction (a, 0x34842000);   // ori a0, 0x2000 - address of BMXCON
        xfer_instruction (a, 0x3c05001f);   // lui a1, 0x1f
        xfer_instruction (a, 0x34a50040);   // ori a1, 0x40   - a1 has 001f0040
        xfer_instruction (a, 0xac850000);   // sw  a1, 0(a0)  - BMXCON initialized
        printf ("1");

        /* Step 2. */
        xfer_instruction (a, 0x34050800);   // li  a1, 0x800  - a1 has 00000800
        xfer_instruction (a, 0xac850010);   // sw  a1, 16(a0) - BMXDKPBA initialized
        printf (" 2");

        /* Step 3. */
        xfer_instruction (a, 0x8c850040);   // lw  a1, 64(a0) - load BMXDMSZ
        xfer_instruction (a, 0xac850020);   // sw  a1, 32(a0) - BMXDUDBA initialized
        xfer_instruction (a, 0xac850030);   // sw  a1, 48(a0) - BMXDUPBA initialized
        printf (" 3");
    }

    /* Step 4. */
    xfer_instruction (a, 0x3c04a000);   // lui a0, 0xa000
    xfer_instruction (a, 0x34840800);   // ori a0, 0x800  - a0 has a0000800
    printf (" 4 (LDR)");

    /* Download the PE loader. */
    int i;
    for (i = 0; i < PIC32_PE_LOADER_LEN; i += 2) {
        /* Step 5. */
        unsigned opcode1 = 0x3c060000 | pic32_pe_loader[i];
        unsigned opcode2 = 0x34c60000 | pic32_pe_loader[i+1];

        xfer_instruction (a, opcode1);      // lui a2, PE_loader_hi++
        xfer_instruction (a, opcode2);      // ori a2, PE_loader_lo++
        xfer_instruction (a, 0xac860000);   // sw  a2, 0(a0)
        xfer_instruction (a, 0x24840004);   // addiu a0, 4
    }
    printf (" 5");

    /* Jump to PE loader (step 6). */
    xfer_instruction (a, 0x3c19a000);   // lui t9, 0xa000
    xfer_instruction (a, 0x37390800);   // ori t9, 0x800  - t9 has a0000800
    xfer_instruction (a, 0x03200008);   // jr  t9
    xfer_instruction (a, 0x00000000);   // nop
    printf (" 6");

    /* Switch from serial to fast execution mode. */
    //bitbang_send (a, 1, 1, 5, TAP_SW_ETAP, 0);
    //bitbang_send (a, 6, 31, 0, 0, 0);             /* TMS 1-1-1-1-1-0 */

    //
    // the above two lines are not present in the Microchip programming specs
    //

    /* Send parameters for the loader (step 7-A).
     * PE_ADDRESS = 0xA000_0900,
     * PE_SIZE */
    bitbang_send (a, 1, 1, 5, ETAP_FASTDATA, 0);  /* Send command. */
    xfer_fastdata (a, 0xa0000900);
    xfer_fastdata (a, nwords);
    printf (" 7a (PE)");

    /* Download the PE itself (step 7-B). */
    for (i = 0; i < nwords; i++) {
        xfer_fastdata (a, *pe++);
    }
    bitbang_delay10mS (a, 3);
    printf (" 7b");

    /* Download the PE instructions. */
    xfer_fastdata (a, 0);                       /* Step 8 - jump to PE. */
    xfer_fastdata (a, 0xDEAD0000);
    bitbang_delay10mS (a, 3);
    printf (" 8");

    xfer_fastdata (a, PE_EXEC_VERSION << 16);

    unsigned version = get_pe_response (a);
    if (version != (PE_EXEC_VERSION << 16 | pe_version)) {
        fprintf (stderr, "\nbad PE version = %08x, expected %08x\n",
                       version, PE_EXEC_VERSION << 16 | pe_version);
        exit (-1);
    }

    printf (" v%04x\n", version & 0xFFFF);

    if (debug_level > 0)
        fprintf (stderr, "PE version = %04x\n", version & 0xffff);
}

/*
 * Erase all flash memory.
 */
static void bitbang_erase_chip (adapter_t *adapter)
{
    bitbang_adapter_t *a = (bitbang_adapter_t*) adapter;

    if (DBG2)
        fprintf (stderr, "erase_chip\n");

    bitbang_send (a, 1, 1, 5, TAP_SW_MTAP, 0);    /* Send command. */
    bitbang_send (a, 1, 1, 5, MTAP_COMMAND, 0);   /* Send command. */
    bitbang_send (a, 0, 0, 8, MCHP_ERASE, 0);     /* Xfer data. */

    if (memcmp(a->adapter.family_name, "mz", 2) == 0)
        bitbang_send (a, 0, 0, 8, MCHP_DEASSERT_RST, 0);      // needed for PIC32MZ devices only.

    int i = 0;
    unsigned status;
    do {
        bitbang_delay10mS (a, 0);
        bitbang_send (a, 0, 0, 8, MCHP_STATUS, 1);    /* Xfer data. */
        status = bitbang_recv (a);
        i++;
    } while ((status & (MCHP_STATUS_CFGRDY |
                        MCHP_STATUS_FCBUSY)) != MCHP_STATUS_CFGRDY && i < 100);

    if (i == 100) {
        fprintf (stderr, "invalid status = %04x (in erase chip)\n", status);
        exit (-1);
    }
    printf ("(%imS) ", i * 10);
}

/*
 * Write a word to flash memory. (only seems to be used to write the four configuration words)
 *
 * !!!!!!!!!! WARNING !!!!!!!!!!
 * on PIC32MZ EC family devices PE_WORD_PROGRAM will not generate the ECC parity bits;
 * instead should use QUAD_WORD_PGRM to program all four configuration words at once.
 * !!!!!!!!!! WARNING !!!!!!!!!!
 */
static void bitbang_program_word (adapter_t *adapter,
    unsigned addr, unsigned word)
{
    bitbang_adapter_t *a = (bitbang_adapter_t*) adapter;

    if (DBG2)
        fprintf (stderr, "program_word\n");

    if (debug_level > 0)
        fprintf (stderr, "program word at %08x: %08x\n", addr, word);
    if (! a->use_executive) {
        /* Without PE. */
        fprintf (stderr, "slow flash write not implemented yet\n");
        exit (-1);
    }

    if (memcmp(a->adapter.family_name, "mz", 2) == 0)
        printf("!ECC!");                        // warn if word-write to MZ processor

    /* Use PE to write flash memory. */
    bitbang_send (a, 1, 1, 5, ETAP_FASTDATA, 0);  /* Send command. */
    xfer_fastdata (a, PE_WORD_PROGRAM << 16 | 2);
    xfer_fastdata (a, addr);                    /* Send address. */
    xfer_fastdata (a, word);                    /* Send word. */

    unsigned response = get_pe_response (a);
    if (response != (PE_WORD_PROGRAM << 16)) {
        fprintf (stderr, "\nfailed to program word %08x at %08x, reply = %08x\n",
                                                   word,   addr,       response);
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

    if (DBG2)
        fprintf (stderr, "program_row\n");
    if (DBG3)
        fprintf (stderr, "\nprogramming %u words at %08x ",
                                   words_per_row,   addr);

    if (debug_level > 0)
        fprintf (stderr, "row program %u words at %08x\n", words_per_row, addr);
    if (! a->use_executive) {
        /* Without PE. */
        fprintf (stderr, "slow flash write not implemented yet\n");
        exit (-1);
    }

    /* Use PE to write flash memory. */
    bitbang_send (a, 1, 1, 5, ETAP_FASTDATA, 0);  /* Send command. */
    xfer_fastdata (a, PE_ROW_PROGRAM << 16 | words_per_row);
    xfer_fastdata (a, addr);                      /* Send address. */

    //
    // The below for loop seems to be where our biggest programming time bottleneck is.
    //

    /* Download data. */
    int i;
    for (i = 0; i < words_per_row; i++) {
        xfer_fastdata (a, *data++);               /* Send word. */
    }

    unsigned response = get_pe_response (a);
    if (response != (PE_ROW_PROGRAM << 16)) {
        fprintf (stderr, "\nfailed to program row at %08x, reply = %08x\n",
                                                     addr,         response);
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

    if (DBG2)
        fprintf (stderr, "verify_data\n");
    if (DBG3)
        fprintf (stderr, "\nverifying %u words at %08x ", nwords, addr);

    if (! a->use_executive) {
        /* Without PE. */
        fprintf (stderr, "slow verify not implemented yet\n");
        exit (-1);
    }

    /* Use PE to get CRC of flash memory. */
    bitbang_send (a, 1, 1, 5, ETAP_FASTDATA, 0);  /* Send command. */
    xfer_fastdata (a, PE_GET_CRC << 16);
    xfer_fastdata (a, addr);                      /* Send address. */
    xfer_fastdata (a, nwords * 4);                /* Send length. */

    unsigned response = get_pe_response (a);
    if (response != (PE_GET_CRC << 16)) {
        fprintf (stderr, "\nfailed to verify %d words at %08x, reply = %08x\n",
                                             nwords,     addr,       response);
        exit (-1);
    }

    flash_crc = get_pe_response (a) & 0xffff;

    data_crc = calculate_crc (0xffff, (unsigned char*) data, nwords * 4);
    if (flash_crc != data_crc) {
        fprintf (stderr, "\nchecksum failed at %08x: returned %04x, expected %04x\n",
                                               addr,        flash_crc,     data_crc);
        exit (-1);
    }
}

/*
 * Initialize bitbang adapter.
 * Return a pointer to a data structure, allocated dynamically.
 * When adapter not found, return 0.
 */
adapter_t *adapter_open_bitbang (const char *port, int baud_rate)
{
    bitbang_adapter_t *a;

    printf ("       (ascii ICSP coded by Robert Rozee)\n\n");

//
// the following block of code is used to upload firmware to
// an attached Arduino NANO using the STK500v1 protocol:
// PIC32PROG -d ASCII:COM5 -b3
//
// once the firmware upload is completed, exits from pic32prog
//

    if (baud_rate < 5)                                  // invoked by a baud rate of 1, 2, 3, or 4.
    {
#if 1                                                   // change to 0 to not compile in firmware uplaod
        #define STK_GET_SYNC            0x30            // synchronize
        #define STK_ENTER_PROGMODE      0x50            // enter program mode
        #define STK_READ_SIGN           0x75            // read signature
        #define STK_LOAD_ADDRESS        0x55            // load address
        #define STK_PROG_PAGE           0x64            // program page
        #define STK_LEAVE_PROGMODE      0x51            // exit program mode

        #define CRC_EOP                 0x20            // end of packet

        #define STK_INSYNC              0x14            // response - insync
        #define STK_OK                  0x10            // response - OK

        #include "bitbang/ICSP_v1C.inc"

        int i, n;
        unsigned char buffer [140];                     // 0x80 + 12d (max used is 133)
        int bps[] = {0, 9600, 19200, 57600, 115200 };   // known arduino bootloader baud rates

        if (serial_open (port, bps[baud_rate], 100) < 0) {
            fprintf (stderr, "Unable to configure serial port %s\n", port);
            serial_close();
            exit (-1);
        }
        printf("%i baud ", bps[baud_rate]);

        for (i = 0; i < 40; i++) {

            buffer[0] = STK_GET_SYNC;                   // get synchronization
            buffer[1] = CRC_EOP;

            serial_write (buffer, 2);
            printf (".");
            n = serial_read (buffer, 2);
            if ((n == 2) && (buffer[0] == STK_INSYNC) && (buffer[1] == STK_OK))
                i = 100;
        }

        if (i < 100) {
            fprintf (stderr, "\nFailed to find arduino/STK500 bootloader\n");
            serial_close();
            exit (-1);
        }
        printf (" synchronized\n");

        buffer[0] = STK_ENTER_PROGMODE;                 // enter program mode (not needed)
        buffer[1] = CRC_EOP;
        serial_write (buffer, 2);
        serial_read (buffer, 2);

        if ((n != 2) || (buffer[0] != STK_INSYNC) || (buffer[1] != STK_OK)) {
            fprintf (stderr, "Failed to enter program mode\n");
            serial_close();
            exit (-1);
        }

        buffer[0] = STK_READ_SIGN;                      // read signature bytes (3)
        buffer[1] = CRC_EOP;
        serial_write (buffer, 2);
        n = serial_read (buffer, 5);

        if ((n != 5) || (buffer[0] != STK_INSYNC) || (buffer[4] != STK_OK)) {
            fprintf (stderr, "Failed to get signature\n");
            serial_close();
            exit (-1);
        }
        unsigned ID = (buffer[1] << 16) + (buffer[2] << 8) + buffer[3];
        printf ("Signature = %06x   Device = %s\n", ID, ID == 0x1e950f ? "ATmega328P" : "(wrong uP)");

        for (i = 0; i < sizeof(ICSP); i += 0x80)
            printf (".");
        for (i = 0; i < sizeof(ICSP); i += 0x80)
            printf ("\b");

        for (i = 0; i < sizeof(ICSP); i += 0x80) {
            printf ("#");

            buffer[0] = STK_LOAD_ADDRESS;               // load address
            buffer[1] = (i >> 1) % 0x100;               // address low (word boundary)
            buffer[2] = (i >> 1) / 0x100;               // address high
            buffer[3] = CRC_EOP;
            serial_write (buffer, 4);
            n = serial_read (buffer, 2);

            if ((n != 2) || (buffer[0] != STK_INSYNC) || (buffer[1] != STK_OK)) {
                fprintf (stderr, "\nFailed to load address %04x\n", i);
                serial_close();
                exit (-1);
            }

            buffer[0] = STK_PROG_PAGE;                  // program page
            buffer[1] = 0x00;                           // length high (in bytes, NOT words)
            buffer[2] = 0x80;                           // length low (order reverse to address)
            buffer[3] = 'F';                            // memory type: 'E' = eeprom, 'F' = flash
            memcpy (&buffer[4], &ICSP[i], 0x80);        // data (128 bytes)
            buffer[4 + 0x80] = CRC_EOP;
            serial_write (buffer, 4 + 0x80 + 1);
            n = serial_read (buffer, 2);

            if ((n != 2) || (buffer[0] != STK_INSYNC) || (buffer[1] != STK_OK)) {
                fprintf (stderr, "\nFailed to program page\n");
                serial_close();
                exit (-1);
            }
        }
        printf ("\n");

        buffer[0] = STK_LEAVE_PROGMODE;                 // leave program mode
        buffer[1] = CRC_EOP;
        serial_write (buffer, 2);
        n = serial_read (buffer, 2);

        if ((n != 2) || (buffer[0] != STK_INSYNC) || (buffer[1] != STK_OK)) {
            fprintf (stderr, "Failed to exit program mode\n");
            serial_close();
            exit (-1);
        }
        printf ("Firmware uploaded to 'ascii ICSP' adapter OK\n");
#else
        printf ("Firmware upload to arduino/STK500 not included\n");
#endif
        serial_close();
        exit (0);                                       // finished performing function, exit program
    }

//
// carry on with normal startup
//

    a = calloc (1, sizeof (*a));
    if (! a) {
        fprintf (stderr, "Out of memory (in open)\n");
        return 0;
    }

    /* Open serial port */
    if (serial_open (port, 500000, 250) < 0) {
        /* failed to open serial port */
        fprintf (stderr, "Unable to configure serial port %s\n", port);
        serial_close();
        free (a);
        return 0;
    }
    usleep (200000);

    //
    // Serial port now open and configured.
    //

    printf ("      Adapter: ");
    int i, n;
    unsigned char ch;
    for (i = 0; i < 20; i++) {
        ch = '>';
        serial_write (&ch, 1);
        printf (".");
        n = serial_read (&ch, 1);
        if (n == 1 && ch == '<')
            i = 100;
    }

    if (i < 100) {
        fprintf (stderr, "\nNo response from 'ascii ICSP' adapter\n");
        serial_close();
        free (a);
        return 0;
    }
    printf(" OK1");

    ch = '?';
    unsigned char buffer[15] = "..............\0";
                            // "ascii ICSP v1C"
    serial_write (&ch, 1);
    n = serial_read (buffer, 14);

    if (n == 14 && memcmp(buffer, "ascii ICSP v1", 13) == 0)
        printf (" OK2 - %s\n", buffer);
    else {
        fprintf (stderr, "\nBad response from 'ascii ICSP' adapter\n");
        serial_close();
        free (a);
        return 0;
    }

    //
    // This is the end of 'ascii ICSP' ID probe
    //

    a->BitsToRead = 0;
    a->PendingHandshake = 0;               // handshake read req'd before next write

    a->TotalBitPairsSent = 0;              // count of total # of TDI+TMS bits sent
    a->TotalBitsReceived = 0;              // count of total # of TDO bits recieved
    a->MaxBufferedWrites = 0;              // maximum continuous write length (chars)
    a->RunningWriteCount = 0;              // running count of writes, reset by read

    a->WriteCount = 0;
    a->Read1Count = 0;
    a->Read2Count = 0;
    a->FDataCount = 0;
    for (i = 0; i < 4; i++)
        a->DelayCount[i] = 0;

    a->use_executive = 0;
    a->serial_execution_mode = 0;

    //
    // It is at this point that we start talking to the target.
    //

    bitbang_ICSP_enable (a, 1);
    gettimeofday (&a->T1, 0);

    //
    // The below few lines enable a forced "emergency" erase,
    // use -b8 for MX processors, -b9 for MZ processors
    //
    // once the erase is completed, exits from pic32prog
    //

    if ((baud_rate >> 1) == 4) {                      // 8 (MX) or 9 (MZ)
        printf ("\nAttempting blind erase of %s processor\n",
            (baud_rate & 1) ? "MZ" : "MX");
        bitbang_send (a, 6, 31, 0, 0, 0);             // don't care about ID
        bitbang_send (a, 1, 1, 5, TAP_SW_MTAP, 0);    /* Send command. */
        bitbang_send (a, 1, 1, 5, MTAP_COMMAND, 0);   /* Send command. */
        bitbang_send (a, 0, 0, 8, MCHP_ERASE, 0);     /* Xfer data. */
        if (baud_rate & 1)
            bitbang_send (a, 0, 0, 8, MCHP_DEASSERT_RST, 0);    // PIC32MZ devices only.
        bitbang_delay10mS (a, 0);
        bitbang_send (a, 0, 0, 8, MCHP_STATUS, 1);    /* Xfer data. */
        usleep (1000000);                             // allow 1 second for erase to complete
        bitbang_ICSP_enable (a, 0);                   // shut down target
        serial_close();
        free (a);
        exit (0);                                     // finished performing function, exit program
    }

    /* Reset the JTAG TAP controller: TMS 1-1-1-1-1-0.
     * After reset, the IDCODE register is always selected.
     * Read out 32 bits of data. */
    unsigned idcode;
    bitbang_send (a, 6, 31, 32, 0, 1);                                       // 1. (pg 20)
    idcode = bitbang_recv (a);
    if ((idcode & 0xfff) != 0x053) {
        /* Microchip vendor ID is expected. */
        if (debug_level > 0 || (idcode != 0 && idcode != 0xffffffff))
            fprintf (stderr, "incompatible CPU detected, IDCODE=%08x\n", idcode);
        bitbang_ICSP_enable (a, 0);              // shut down target
        serial_close();
        free (a);
        return 0;
    }

    /* Check status. */
    bitbang_send (a, 1, 1, 5, TAP_SW_MTAP, 0);      /* Send command. */      // 2.
    bitbang_send (a, 1, 1, 5, MTAP_COMMAND, 0);     /* Send command. */      // 3.
#ifdef OLDWAY
    bitbang_send (a, 0, 0, 8, MCHP_FLASH_ENABLE, 0); /* Xfer data. */        // may be an issue for MZ family
    // (above line) "This command requires a NOP to complete."
#else
    bitbang_send (a, 0, 0, 8, MCHP_STATUS, 0);      /* Xfer data. */         // 4a.
    bitbang_delay10mS (a, 3);
#endif
    bitbang_send (a, 0, 0, 8, MCHP_STATUS, 1);      /* Xfer data. */         // 4b.
    unsigned status = bitbang_recv (a);
    if (debug_level > 0)
        fprintf (stderr, "status %04x\n", status);
#ifdef OLDWAY
    if ((status & ~MCHP_STATUS_DEVRST) !=
        (MCHP_STATUS_CPS | MCHP_STATUS_CFGRDY | MCHP_STATUS_FAEN)) {
#else
    if ((status & (MCHP_STATUS_CFGRDY | MCHP_STATUS_FCBUSY)) != MCHP_STATUS_CFGRDY) {
#endif
        fprintf (stderr, "invalid status = %04x (in open)\n", status);       // 5.
        bitbang_ICSP_enable (a, 0);              // shut down target
        serial_close();
        free (a);
        return 0;
    }
//
// the above two blocks of code that get the device ID and check status have been altered to
// bring into line with the current microchip recommendations. this also removes a potential
// incompatibility with MZ family devices, which do not support MCHP_FLASH_ENABLE.
//
// see: Microchip document 60001145N, "PIC32 Flash Programming
// Specification, DS60001145N page 20 (8.0 CHECK DEVICE STATUS)
//
// to return to old method, add line #define OLDWAY
//

    a->adapter.flags = AD_PROBE | AD_ERASE | AD_READ | AD_WRITE;

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
