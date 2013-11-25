/*
 * Interface to PIC32 microcontroller via debug port.
 *
 * Copyright (C) 2011-2013 Serge Vakulenko
 *
 * This file is part of PIC32PROG project, which is distributed
 * under the terms of the GNU General Public License (GPL).
 * See the accompanying file "COPYING" for more details.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>

#include "target.h"
#include "adapter.h"
#include "localize.h"
#include "pic32.h"

typedef void print_func_t (target_t *t, unsigned cfg0,
    unsigned cfg1, unsigned cfg2, unsigned cfg3);

typedef struct {
    unsigned        boot_kbytes;
    unsigned        devcfg_offset;
    unsigned        bytes_per_row;
    print_func_t    *print_devcfg;
    const unsigned  *pe_code;
    unsigned        pe_nwords;
    unsigned        pe_version;
} family_t;

struct _target_t {
    adapter_t       *adapter;
    const char      *cpu_name;
    const family_t  *family;
    unsigned        cpuid;
    unsigned        flash_addr;
    unsigned        flash_bytes;
};

static print_func_t print_mx1;
static print_func_t print_mx3;
static print_func_t print_mz;

                    /*-Boot-Devcfg--Row---Print------Code--------Nwords-Version-*/
static const
family_t family_mx1 = { 3,  0x0bf0, 128,  print_mx1, pic32_pemx1, 422,  0x0301 };
static const
family_t family_mx3 = { 12, 0x2ff0, 512,  print_mx3, pic32_pemx3, 1044, 0x0201 };
static const
family_t family_mz  = { 80, 0xffc0, 2048, print_mz,  pic32_pemz,  1052, 0x0502 };

static const struct {
    unsigned        devid;
    const char      *name;
    unsigned        flash_kbytes;
    const family_t  *family;
} pic32mx_dev[] = {
    /* MX1/2 family-------------Flash---Family */
    {0x4A07053, "MX110F016B",     16,   &family_mx1},
    {0x4A09053, "MX110F016C",     16,   &family_mx1},
    {0x4A0B053, "MX110F016D",     16,   &family_mx1},
    {0x4A06053, "MX120F032B",     32,   &family_mx1},
    {0x4A08053, "MX120F032C",     32,   &family_mx1},
    {0x4A0A053, "MX120F032D",     32,   &family_mx1},
    {0x4D07053, "MX130F064B",     64,   &family_mx1},
    {0x4D09053, "MX130F064C",     64,   &family_mx1},
    {0x4D0B053, "MX130F064D",     64,   &family_mx1},
    {0x4D06053, "MX150F128B",    128,   &family_mx1},
    {0x4D08053, "MX150F128C",    128,   &family_mx1},
    {0x4D0A053, "MX150F128D",    128,   &family_mx1},
    {0x4A01053, "MX210F016B",     16,   &family_mx1},
    {0x4A03053, "MX210F016C",     16,   &family_mx1},
    {0x4A05053, "MX210F016D",     16,   &family_mx1},
    {0x4A00053, "MX220F032B",     32,   &family_mx1},
    {0x4A02053, "MX220F032C",     32,   &family_mx1},
    {0x4A04053, "MX220F032D",     32,   &family_mx1},
    {0x4D01053, "MX230F064B",     64,   &family_mx1},
    {0x4D03053, "MX230F064C",     64,   &family_mx1},
    {0x4D05053, "MX230F064D",     64,   &family_mx1},
    {0x4D00053, "MX250F128B",    128,   &family_mx1},
    {0x4D02053, "MX250F128C",    128,   &family_mx1},
    {0x4D04053, "MX250F128D",    128,   &family_mx1},

    /* MX3/4/5/6/7 family-------Flash---Family */
    {0x0902053, "MX320F032H",     32,   &family_mx3},
    {0x0906053, "MX320F064H",     64,   &family_mx3},
    {0x090A053, "MX320F128H",    128,   &family_mx3},
    {0x092A053, "MX320F128L",    128,   &family_mx3},
    {0x5600053, "MX330F064H",     64,   &family_mx3},
    {0x5601053, "MX330F064L",     64,   &family_mx3},
    {0x090D053, "MX340F128H",    128,   &family_mx3},
    {0x092D053, "MX340F128L",    128,   &family_mx3},
    {0x0912053, "MX340F256H",    256,   &family_mx3},
    {0x0916053, "MX340F512H",    512,   &family_mx3},
    {0x570C053, "MX350F128H",    128,   &family_mx3},
    {0x570D053, "MX350F128L",    128,   &family_mx3},
    {0x5704053, "MX350F256H",    256,   &family_mx3},
    {0x5705053, "MX350F256L",    256,   &family_mx3},
    {0x0934053, "MX360F256L",    256,   &family_mx3},
    {0x0938053, "MX360F512L",    512,   &family_mx3},
    {0x5808053, "MX370F512H",    512,   &family_mx3},
    {0x5809053, "MX370F512L",    512,   &family_mx3},
    {0x0942053, "MX420F032H",     32,   &family_mx3},
    {0x5602053, "MX430F064H",     64,   &family_mx3},
    {0x5603053, "MX430F064L",     64,   &family_mx3},
    {0x094D053, "MX440F128H",    128,   &family_mx3},
    {0x096D053, "MX440F128L",    128,   &family_mx3},
    {0x0952053, "MX440F256H",    256,   &family_mx3},
    {0x0956053, "MX440F512H",    512,   &family_mx3},
    {0x570E053, "MX450F128H",    128,   &family_mx3},
    {0x570F053, "MX450F128L",    128,   &family_mx3},
    {0x5706053, "MX450F256H",    256,   &family_mx3},
    {0x5707053, "MX450F256L",    256,   &family_mx3},
    {0x0974053, "MX460F256L",    256,   &family_mx3},
    {0x0978053, "MX460F512L",    512,   &family_mx3},
    {0x580A053, "MX470F512H",    512,   &family_mx3},
    {0x580B053, "MX470F512L",    512,   &family_mx3},
    {0x4400053, "MX534F064H",     64,   &family_mx3},
    {0x440C053, "MX534F064L",     64,   &family_mx3},
    {0x4401053, "MX564F064H",     64,   &family_mx3},
    {0x440D053, "MX564F064L",     64,   &family_mx3},
    {0x4403053, "MX564F128H",    128,   &family_mx3},
    {0x440F053, "MX564F128L",    128,   &family_mx3},
    {0x4317053, "MX575F256H",    256,   &family_mx3},
    {0x4333053, "MX575F256L",    256,   &family_mx3},
    {0x4309053, "MX575F512H",    512,   &family_mx3},
    {0x430F053, "MX575F512L",    512,   &family_mx3},
    {0x4405053, "MX664F064H",     64,   &family_mx3},
    {0x4411053, "MX664F064L",     64,   &family_mx3},
    {0x4407053, "MX664F128H",    128,   &family_mx3},
    {0x4413053, "MX664F128L",    128,   &family_mx3},
    {0x430B053, "MX675F256H",    256,   &family_mx3},
    {0x4305053, "MX675F256L",    256,   &family_mx3},
    {0x430C053, "MX675F512H",    512,   &family_mx3},
    {0x4311053, "MX675F512L",    512,   &family_mx3},
    {0x4325053, "MX695F512H",    512,   &family_mx3},
    {0x4341053, "MX695F512L",    512,   &family_mx3},
    {0x440B053, "MX764F128H",    128,   &family_mx3},
    {0x4417053, "MX764F128L",    128,   &family_mx3},
    {0x4303053, "MX775F256H",    256,   &family_mx3},
    {0x4312053, "MX775F256L",    256,   &family_mx3},
    {0x430D053, "MX775F512H",    512,   &family_mx3},
    {0x4306053, "MX775F512L",    512,   &family_mx3},
    {0x430E053, "MX795F512H",    512,   &family_mx3},
    {0x4307053, "MX795F512L",    512,   &family_mx3},

    /* MZ family----------------Flash---Family */
    {0x5100053, "MZ0256ECE064",  256,   &family_mz},
    {0x510A053, "MZ0256ECE100",  256,   &family_mz},
    {0x5114053, "MZ0256ECE124",  256,   &family_mz},
    {0x511E053, "MZ0256ECE144",  256,   &family_mz},
    {0x5105053, "MZ0256ECF064",  256,   &family_mz},
    {0x510F053, "MZ0256ECF100",  256,   &family_mz},
    {0x5119053, "MZ0256ECF124",  256,   &family_mz},
    {0x5123053, "MZ0256ECF144",  256,   &family_mz},
    {0x5101053, "MZ0512ECE064",  512,   &family_mz},
    {0x510B053, "MZ0512ECE100",  512,   &family_mz},
    {0x5115053, "MZ0512ECE124",  512,   &family_mz},
    {0x511F053, "MZ0512ECE144",  512,   &family_mz},
    {0x5106053, "MZ0512ECF064",  512,   &family_mz},
    {0x5110053, "MZ0512ECF100",  512,   &family_mz},
    {0x511A053, "MZ0512ECF124",  512,   &family_mz},
    {0x5124053, "MZ0512ECF144",  512,   &family_mz},
    {0x5102053, "MZ1024ECE064", 1024,   &family_mz},
    {0x510C053, "MZ1024ECE100", 1024,   &family_mz},
    {0x5116053, "MZ1024ECE124", 1024,   &family_mz},
    {0x5120053, "MZ1024ECE144", 1024,   &family_mz},
    {0x5107053, "MZ1024ECF064", 1024,   &family_mz},
    {0x5111053, "MZ1024ECF100", 1024,   &family_mz},
    {0x511B053, "MZ1024ECF124", 1024,   &family_mz},
    {0x5125053, "MZ1024ECF144", 1024,   &family_mz},
    {0x5103053, "MZ1024ECG064", 1024,   &family_mz},
    {0x510D053, "MZ1024ECG100", 1024,   &family_mz},
    {0x5117053, "MZ1024ECG124", 1024,   &family_mz},
    {0x5121053, "MZ1024ECG144", 1024,   &family_mz},
    {0x5108053, "MZ1024ECH064", 1024,   &family_mz},
    {0x5112053, "MZ1024ECH100", 1024,   &family_mz},
    {0x511C053, "MZ1024ECH124", 1024,   &family_mz},
    {0x5126053, "MZ1024ECH144", 1024,   &family_mz},
    {0x5130053, "MZ1024ECM064", 1024,   &family_mz},
    {0x513A053, "MZ1024ECM100", 1024,   &family_mz},
    {0x5144053, "MZ1024ECM124", 1024,   &family_mz},
    {0x514E053, "MZ1024ECM144", 1024,   &family_mz},
    {0x5104053, "MZ2048ECG064", 2048,   &family_mz},
    {0x510E053, "MZ2048ECG100", 2048,   &family_mz},
    {0x5118053, "MZ2048ECG124", 2048,   &family_mz},
    {0x5122053, "MZ2048ECG144", 2048,   &family_mz},
    {0x5109053, "MZ2048ECH064", 2048,   &family_mz},
    {0x5113053, "MZ2048ECH100", 2048,   &family_mz},
    {0x511D053, "MZ2048ECH124", 2048,   &family_mz},
    {0x5127053, "MZ2048ECH144", 2048,   &family_mz},
    {0x5131053, "MZ2048ECM064", 2048,   &family_mz},
    {0x513B053, "MZ2048ECM100", 2048,   &family_mz},
    {0x5145053, "MZ2048ECM124", 2048,   &family_mz},
    {0x514F053, "MZ2048ECM144", 2048,   &family_mz},

    /* USB bootloader */
    {0xEAFB00B, "Bootloader",   0,      0},
    {0}
};

#if defined (__CYGWIN32__) || defined (MINGW32)
/*
 * Delay in milliseconds: Windows.
 */
#include <windows.h>

void mdelay (unsigned msec)
{
    Sleep (msec);
}
#else
/*
 * Delay in milliseconds: Unix.
 */
void mdelay (unsigned msec)
{
    usleep (msec * 1000);
}
#endif

/*
 * Connect to JTAG adapter.
 */
target_t *target_open ()
{
    target_t *t;

    t = calloc (1, sizeof (target_t));
    if (! t) {
        fprintf (stderr, _("Out of memory\n"));
        exit (-1);
    }
    t->cpu_name = "Unknown";

    /* Find adapter. */
    t->adapter = adapter_open_pickit ();
#ifdef USE_MPSSE
    if (! t->adapter)
        t->adapter = adapter_open_mpsse ();
#endif
    if (! t->adapter)
        t->adapter = adapter_open_hidboot ();
    if (! t->adapter)
        t->adapter = adapter_open_an1388 ();
    if (! t->adapter) {
        fprintf (stderr, "\n");
        fprintf (stderr, _("No target found.\n"));
        exit (-1);
    }

    /* Check CPU identifier. */
    t->cpuid = t->adapter->get_idcode (t->adapter);
    if (t->cpuid == 0) {
        /* Device not responding. */
        fprintf (stderr, _("Unknown CPUID=%08x.\n"), t->cpuid);
        t->adapter->close (t->adapter, 0);
        exit (1);
    }

    unsigned i;
    for (i=0; (t->cpuid ^ pic32mx_dev[i].devid) & 0x0fffffff; i++) {
        if (pic32mx_dev[i].devid == 0) {
            /* Device not detected. */
            fprintf (stderr, _("Unknown CPUID=%08x.\n"), t->cpuid);
            t->adapter->close (t->adapter, 0);
            exit (1);
        }
    }
    t->family = pic32mx_dev[i].family;
    t->cpu_name = pic32mx_dev[i].name;
    t->flash_addr = 0x1d000000;
    t->flash_bytes = pic32mx_dev[i].flash_kbytes * 1024;
    if (! t->flash_bytes) {
        t->flash_addr = t->adapter->user_start;
        t->flash_bytes = t->adapter->user_nbytes;
    }
    return t;
}

/*
 * Close the device.
 */
void target_close (target_t *t, int power_on)
{
    t->adapter->close (t->adapter, power_on);
}

const char *target_cpu_name (target_t *t)
{
    return t->cpu_name;
}

unsigned target_idcode (target_t *t)
{
    return t->cpuid;
}

unsigned target_flash_bytes (target_t *t)
{
    return t->flash_bytes;
}

unsigned target_boot_bytes (target_t *t)
{
    return t->family->boot_kbytes * 1024;
}

unsigned target_devcfg_offset (target_t *t)
{
    return t->family->devcfg_offset;
}

unsigned target_block_size (target_t *t)
{
    return t->family->bytes_per_row;
}

/*
 * Use PE for reading/writing/erasing memory.
 */
void target_use_executive (target_t *t)
{
    if (t->adapter->load_executive != 0)
        t->adapter->load_executive (t->adapter, t->family->pe_code,
            t->family->pe_nwords, t->family->pe_version);
}

/*
 * Print configuration for MX3/4/5/6/7 family.
 */
static void print_mx3 (target_t *t, unsigned cfg0,
    unsigned cfg1, unsigned cfg2, unsigned cfg3)
{
    /*--------------------------------------
     * Configuration register 0
     */
    printf ("    DEVCFG0 = %08x\n", cfg0);
    if ((~cfg0 & MX3_CFG0_DEBUG_MASK) == MX3_CFG0_DEBUG_ENABLED)
        printf ("                     %u Debugger enabled\n",
            cfg0 & MX3_CFG0_DEBUG_MASK);
    else
        printf ("                     %u Debugger disabled\n",
            cfg0 & MX3_CFG0_DEBUG_MASK);

    switch (~cfg0 & MX3_CFG0_ICESEL_MASK) {
    case MX3_CFG0_ICESEL_PAIR1:
        printf ("                    %02x Use PGC1/PGD1\n", cfg0 & MX3_CFG0_ICESEL_MASK);
        break;
    case MX3_CFG0_ICESEL_PAIR2:
        printf ("                    %02x Use PGC2/PGD2\n", cfg0 & MX3_CFG0_ICESEL_MASK);
        break;
    case MX3_CFG0_ICESEL_PAIR3:
        printf ("                    %02x Use PGC3/PGD3\n", cfg0 & MX3_CFG0_ICESEL_MASK);
        break;
    case MX3_CFG0_ICESEL_PAIR4:
        printf ("                    %02x Use PGC4/PGD4\n", cfg0 & MX3_CFG0_ICESEL_MASK);
        break;
    }

    if (~cfg0 & MX3_CFG0_PWP_MASK)
        printf ("                 %05x Program flash write protect\n",
            cfg0 & MX3_CFG0_PWP_MASK);

    if (~cfg0 & MX3_CFG0_BWP)
        printf ("                       Boot flash write protect\n");
    if (~cfg0 & MX3_CFG0_CP)
        printf ("                       Code protect\n");

    /*--------------------------------------
     * Configuration register 1
     */
    printf ("    DEVCFG1 = %08x\n", cfg1);
    switch (cfg1 & MX3_CFG1_FNOSC_MASK) {
    case MX3_CFG1_FNOSC_FRC:
        printf ("                     %u Fast RC oscillator\n", MX3_CFG1_FNOSC_FRC);
        break;
    case MX3_CFG1_FNOSC_FRCDIVPLL:
        printf ("                     %u Fast RC oscillator with divide-by-N and PLL\n", MX3_CFG1_FNOSC_FRCDIVPLL);
        break;
    case MX3_CFG1_FNOSC_PRI:
        printf ("                     %u Primary oscillator\n", MX3_CFG1_FNOSC_PRI);
        break;
    case MX3_CFG1_FNOSC_PRIPLL:
        printf ("                     %u Primary oscillator with PLL\n", MX3_CFG1_FNOSC_PRIPLL);
        break;
    case MX3_CFG1_FNOSC_SEC:
        printf ("                     %u Secondary oscillator\n", MX3_CFG1_FNOSC_SEC);
        break;
    case MX3_CFG1_FNOSC_LPRC:
        printf ("                     %u Low-power RC oscillator\n", MX3_CFG1_FNOSC_LPRC);
        break;
    case MX3_CFG1_FNOSC_FRCDIV:
        printf ("                     %u Fast RC oscillator with divide-by-N\n", MX3_CFG1_FNOSC_FRCDIV);
        break;
    default:
        printf ("                     %u UNKNOWN\n", cfg1 & MX3_CFG1_FNOSC_MASK);
        break;
    }
    if (cfg1 & MX3_CFG1_FSOSCEN)
        printf ("                    %u  Secondary oscillator enabled\n",
            MX3_CFG1_FSOSCEN >> 4);
    if (cfg1 & MX3_CFG1_IESO)
        printf ("                    %u  Internal-external switch over enabled\n",
            MX3_CFG1_IESO >> 4);

    switch (cfg1 & MX3_CFG1_POSCMOD_MASK) {
    case MX3_CFG1_POSCMOD_EXT:
        printf ("                   %u   Primary oscillator: External\n", MX3_CFG1_POSCMOD_EXT >> 8);
        break;
    case MX3_CFG1_POSCMOD_XT:
        printf ("                   %u   Primary oscillator: XT\n", MX3_CFG1_POSCMOD_XT >> 8);
        break;
    case MX3_CFG1_POSCMOD_HS:
        printf ("                   %u   Primary oscillator: HS\n", MX3_CFG1_POSCMOD_HS >> 8);
        break;
    case MX3_CFG1_POSCMOD_DISABLE:
        printf ("                   %u   Primary oscillator: disabled\n", MX3_CFG1_POSCMOD_DISABLE >> 8);
        break;
    }
    if (! (cfg1 & MX3_CFG1_OSCIOFNC))
        printf ("                   %u   CLKO output active\n",
            MX3_CFG1_OSCIOFNC >> 8);

    switch (cfg1 & MX3_CFG1_FPBDIV_MASK) {
    case MX3_CFG1_FPBDIV_1:
        printf ("                  %u    Peripheral bus clock: SYSCLK / 1\n", MX3_CFG1_FPBDIV_1 >> 12);
        break;
    case MX3_CFG1_FPBDIV_2:
        printf ("                  %u    Peripheral bus clock: SYSCLK / 2\n", MX3_CFG1_FPBDIV_2 >> 12);
        break;
    case MX3_CFG1_FPBDIV_4:
        printf ("                  %u    Peripheral bus clock: SYSCLK / 4\n", MX3_CFG1_FPBDIV_4 >> 12);
        break;
    case MX3_CFG1_FPBDIV_8:
        printf ("                  %u    Peripheral bus clock: SYSCLK / 8\n", MX3_CFG1_FPBDIV_8 >> 12);
        break;
    }
    if (cfg1 & MX3_CFG1_FCKM_DISABLE)
        printf ("                  %u    Fail-safe clock monitor disable\n",
            MX3_CFG1_FCKM_DISABLE >> 12);
    if (cfg1 & MX3_CFG1_FCKS_DISABLE)
        printf ("                  %u    Clock switching disable\n",
            MX3_CFG1_FCKS_DISABLE >> 12);

    switch (cfg1 & MX3_CFG1_WDTPS_MASK) {
    case MX3_CFG1_WDTPS_1:
        printf ("                %2x     Watchdog postscale: 1/1\n", MX3_CFG1_WDTPS_1 >> 16);
        break;
    case MX3_CFG1_WDTPS_2:
        printf ("                %2x     Watchdog postscale: 1/2\n", MX3_CFG1_WDTPS_2 >> 16);
        break;
    case MX3_CFG1_WDTPS_4:
        printf ("                %2x     Watchdog postscale: 1/4\n", MX3_CFG1_WDTPS_4 >> 16);
        break;
    case MX3_CFG1_WDTPS_8:
        printf ("                %2x     Watchdog postscale: 1/8\n", MX3_CFG1_WDTPS_8 >> 16);
        break;
    case MX3_CFG1_WDTPS_16:
        printf ("                %2x     Watchdog postscale: 1/16\n", MX3_CFG1_WDTPS_16 >> 16);
        break;
    case MX3_CFG1_WDTPS_32:
        printf ("                %2x     Watchdog postscale: 1/32\n", MX3_CFG1_WDTPS_32 >> 16);
        break;
    case MX3_CFG1_WDTPS_64:
        printf ("                %2x     Watchdog postscale: 1/64\n", MX3_CFG1_WDTPS_64 >> 16);
        break;
    case MX3_CFG1_WDTPS_128:
        printf ("                %2x     Watchdog postscale: 1/128\n", MX3_CFG1_WDTPS_128 >> 16);
        break;
    case MX3_CFG1_WDTPS_256:
        printf ("                %2x     Watchdog postscale: 1/256\n", MX3_CFG1_WDTPS_256 >> 16);
        break;
    case MX3_CFG1_WDTPS_512:
        printf ("                %2x     Watchdog postscale: 1/512\n", MX3_CFG1_WDTPS_512 >> 16);
        break;
    case MX3_CFG1_WDTPS_1024:
        printf ("                %2x     Watchdog postscale: 1/1024\n", MX3_CFG1_WDTPS_1024 >> 16);
        break;
    case MX3_CFG1_WDTPS_2048:
        printf ("                %2x     Watchdog postscale: 1/2048\n", MX3_CFG1_WDTPS_2048 >> 16);
        break;
    case MX3_CFG1_WDTPS_4096:
        printf ("                %2x     Watchdog postscale: 1/4096\n", MX3_CFG1_WDTPS_4096 >> 16);
        break;
    case MX3_CFG1_WDTPS_8192:
        printf ("                %2x     Watchdog postscale: 1/8192\n", MX3_CFG1_WDTPS_8192 >> 16);
        break;
    case MX3_CFG1_WDTPS_16384:
        printf ("                %2x     Watchdog postscale: 1/16384\n", MX3_CFG1_WDTPS_16384 >> 16);
        break;
    case MX3_CFG1_WDTPS_32768:
        printf ("                %2x     Watchdog postscale: 1/32768\n", MX3_CFG1_WDTPS_32768 >> 16);
        break;
    case MX3_CFG1_WDTPS_65536:
        printf ("                %2x     Watchdog postscale: 1/65536\n", MX3_CFG1_WDTPS_65536 >> 16);
        break;
    case MX3_CFG1_WDTPS_131072:
        printf ("                %2x     Watchdog postscale: 1/131072\n", MX3_CFG1_WDTPS_131072 >> 16);
        break;
    case MX3_CFG1_WDTPS_262144:
        printf ("                %2x     Watchdog postscale: 1/262144\n", MX3_CFG1_WDTPS_262144 >> 16);
        break;
    case MX3_CFG1_WDTPS_524288:
        printf ("                %2x     Watchdog postscale: 1/524288\n", MX3_CFG1_WDTPS_524288 >> 16);
        break;
    case MX3_CFG1_WDTPS_1048576:
        printf ("                %2x     Watchdog postscale: 1/1048576\n", MX3_CFG1_WDTPS_1048576 >> 16);
        break;
    }
    if (cfg1 & MX3_CFG1_FWDTEN)
        printf ("                %u      Watchdog enable\n",
            MX3_CFG1_FWDTEN >> 20);

    /*--------------------------------------
     * Configuration register 2
     */
    printf ("    DEVCFG2 = %08x\n", cfg2);
    switch (cfg2 & MX3_CFG2_FPLLIDIV_MASK) {
    case MX3_CFG2_FPLLIDIV_1:
        printf ("                     %u PLL divider: 1/1\n", MX3_CFG2_FPLLIDIV_1);
        break;
    case MX3_CFG2_FPLLIDIV_2:
        printf ("                     %u PLL divider: 1/2\n", MX3_CFG2_FPLLIDIV_2);
        break;
    case MX3_CFG2_FPLLIDIV_3:
        printf ("                     %u PLL divider: 1/3\n", MX3_CFG2_FPLLIDIV_3);
        break;
    case MX3_CFG2_FPLLIDIV_4:
        printf ("                     %u PLL divider: 1/4\n", MX3_CFG2_FPLLIDIV_4);
        break;
    case MX3_CFG2_FPLLIDIV_5:
        printf ("                     %u PLL divider: 1/5\n", MX3_CFG2_FPLLIDIV_5);
        break;
    case MX3_CFG2_FPLLIDIV_6:
        printf ("                     %u PLL divider: 1/6\n", MX3_CFG2_FPLLIDIV_6);
        break;
    case MX3_CFG2_FPLLIDIV_10:
        printf ("                     %u PLL divider: 1/10\n", MX3_CFG2_FPLLIDIV_10);
        break;
    case MX3_CFG2_FPLLIDIV_12:
        printf ("                     %u PLL divider: 1/12\n", MX3_CFG2_FPLLIDIV_12);
        break;
    }
    switch (cfg2 & MX3_CFG2_FPLLMUL_MASK) {
    case MX3_CFG2_FPLLMUL_15:
        printf ("                    %u  PLL multiplier: 15x\n", MX3_CFG2_FPLLMUL_15 >> 4);
        break;
    case MX3_CFG2_FPLLMUL_16:
        printf ("                    %u  PLL multiplier: 16x\n", MX3_CFG2_FPLLMUL_16 >> 4);
        break;
    case MX3_CFG2_FPLLMUL_17:
        printf ("                    %u  PLL multiplier: 17x\n", MX3_CFG2_FPLLMUL_17 >> 4);
        break;
    case MX3_CFG2_FPLLMUL_18:
        printf ("                    %u  PLL multiplier: 18x\n", MX3_CFG2_FPLLMUL_18 >> 4);
        break;
    case MX3_CFG2_FPLLMUL_19:
        printf ("                    %u  PLL multiplier: 19x\n", MX3_CFG2_FPLLMUL_19 >> 4);
        break;
    case MX3_CFG2_FPLLMUL_20:
        printf ("                    %u  PLL multiplier: 20x\n", MX3_CFG2_FPLLMUL_20 >> 4);
        break;
    case MX3_CFG2_FPLLMUL_21:
        printf ("                    %u  PLL multiplier: 21x\n", MX3_CFG2_FPLLMUL_21 >> 4);
        break;
    case MX3_CFG2_FPLLMUL_24:
        printf ("                    %u  PLL multiplier: 24x\n", MX3_CFG2_FPLLMUL_24 >> 4);
        break;
    }
    switch (cfg2 & MX3_CFG2_UPLLIDIV_MASK) {
    case MX3_CFG2_UPLLIDIV_1:
        printf ("                   %u   USB PLL divider: 1/1\n", MX3_CFG2_UPLLIDIV_1 >> 8);
        break;
    case MX3_CFG2_UPLLIDIV_2:
        printf ("                   %u   USB PLL divider: 1/2\n", MX3_CFG2_UPLLIDIV_2 >> 8);
        break;
    case MX3_CFG2_UPLLIDIV_3:
        printf ("                   %u   USB PLL divider: 1/3\n", MX3_CFG2_UPLLIDIV_3 >> 8);
        break;
    case MX3_CFG2_UPLLIDIV_4:
        printf ("                   %u   USB PLL divider: 1/4\n", MX3_CFG2_UPLLIDIV_4 >> 8);
        break;
    case MX3_CFG2_UPLLIDIV_5:
        printf ("                   %u   USB PLL divider: 1/5\n", MX3_CFG2_UPLLIDIV_5 >> 8);
        break;
    case MX3_CFG2_UPLLIDIV_6:
        printf ("                   %u   USB PLL divider: 1/6\n", MX3_CFG2_UPLLIDIV_6 >> 8);
        break;
    case MX3_CFG2_UPLLIDIV_10:
        printf ("                   %u   USB PLL divider: 1/10\n", MX3_CFG2_UPLLIDIV_10 >> 8);
        break;
    case MX3_CFG2_UPLLIDIV_12:
        printf ("                   %u   USB PLL divider: 1/12\n", MX3_CFG2_UPLLIDIV_12 >> 8);
        break;
    }
    if (cfg2 & MX3_CFG2_UPLL_DISABLE)
        printf ("                  %u    Disable USB PLL\n",
            MX3_CFG2_UPLL_DISABLE >> 12);
    else
        printf ("                       Enable USB PLL\n");

    switch (cfg2 & MX3_CFG2_FPLLODIV_MASK) {
    case MX3_CFG2_FPLLODIV_1:
        printf ("                 %u     PLL postscaler: 1/1\n", MX3_CFG2_FPLLODIV_1 >> 16);
        break;
    case MX3_CFG2_FPLLODIV_2:
        printf ("                 %u     PLL postscaler: 1/2\n", MX3_CFG2_FPLLODIV_2 >> 16);
        break;
    case MX3_CFG2_FPLLODIV_4:
        printf ("                 %u     PLL postscaler: 1/4\n", MX3_CFG2_FPLLODIV_4 >> 16);
        break;
    case MX3_CFG2_FPLLODIV_8:
        printf ("                 %u     PLL postscaler: 1/8\n", MX3_CFG2_FPLLODIV_8 >> 16);
        break;
    case MX3_CFG2_FPLLODIV_16:
        printf ("                 %u     PLL postscaler: 1/16\n", MX3_CFG2_FPLLODIV_16 >> 16);
        break;
    case MX3_CFG2_FPLLODIV_32:
        printf ("                 %u     PLL postscaler: 1/32\n", MX3_CFG2_FPLLODIV_32 >> 16);
        break;
    case MX3_CFG2_FPLLODIV_64:
        printf ("                 %u     PLL postscaler: 1/64\n", MX3_CFG2_FPLLODIV_64 >> 16);
        break;
    case MX3_CFG2_FPLLODIV_256:
        printf ("                 %u     PLL postscaler: 1/128\n", MX3_CFG2_FPLLODIV_256 >> 16);
        break;
    }

    /*--------------------------------------
     * Configuration register 3
     */
    printf ("    DEVCFG3 = %08x\n", cfg3);
    if (~cfg3 & MX3_CFG3_USERID_MASK)
        printf ("                  %04x User-defined ID\n",
            cfg3 & MX3_CFG3_USERID_MASK);

    switch (cfg3 & MX3_CFG3_FSRSSEL_MASK) {
    case MX3_CFG3_FSRSSEL_ALL:
        printf ("                 %u     All irqs assigned to shadow set\n", MX3_CFG3_FSRSSEL_ALL >> 16);
        break;
    case MX3_CFG3_FSRSSEL_1:
        printf ("                 %u     Assign irq priority 1 to shadow set\n", MX3_CFG3_FSRSSEL_1 >> 16);
        break;
    case MX3_CFG3_FSRSSEL_2:
        printf ("                 %u     Assign irq priority 2 to shadow set\n", MX3_CFG3_FSRSSEL_2 >> 16);
        break;
    case MX3_CFG3_FSRSSEL_3:
        printf ("                 %u     Assign irq priority 3 to shadow set\n", MX3_CFG3_FSRSSEL_3 >> 16);
        break;
    case MX3_CFG3_FSRSSEL_4:
        printf ("                 %u     Assign irq priority 4 to shadow set\n", MX3_CFG3_FSRSSEL_4 >> 16);
        break;
    case MX3_CFG3_FSRSSEL_5:
        printf ("                 %u     Assign irq priority 5 to shadow set\n", MX3_CFG3_FSRSSEL_5 >> 16);
        break;
    case MX3_CFG3_FSRSSEL_6:
        printf ("                 %u     Assign irq priority 6 to shadow set\n", MX3_CFG3_FSRSSEL_6 >> 16);
        break;
    case MX3_CFG3_FSRSSEL_7:
        printf ("                 %u     Assign irq priority 7 to shadow set\n", MX3_CFG3_FSRSSEL_7 >> 16);
        break;
    }
    if (cfg3 & MX3_CFG3_FMIIEN)
        printf ("               %u       Ethernet MII enabled\n",
            MX3_CFG3_FMIIEN >> 24);
    else
        printf ("                       Ethernet RMII enabled\n");

    if (cfg3 & MX3_CFG3_FETHIO)
        printf ("               %u       Default Ethernet i/o pins\n",
            MX3_CFG3_FETHIO >> 24);
    else
        printf ("                       Alternate Ethernet i/o pins\n");

    if (cfg3 & MX3_CFG3_FCANIO)
        printf ("               %u       Default CAN i/o pins\n",
            MX3_CFG3_FCANIO >> 24);
    else
        printf ("                       Alternate CAN i/o pins\n");

    if (cfg3 & MX3_CFG3_FUSBIDIO)
        printf ("              %u        USBID pin: controlled by USB\n",
            MX3_CFG3_FUSBIDIO >> 28);
    else
        printf ("                       USBID pin: controlled by port\n");

    if (cfg3 & MX3_CFG3_FVBUSONIO)
        printf ("              %u        VBuson pin: controlled by USB\n",
            MX3_CFG3_FVBUSONIO >> 28);
    else
        printf ("                       VBuson pin: controlled by port\n");
}

/*
 * Print configuration for MX1/2 family.
 */
static void print_mx1 (target_t *t, unsigned cfg0,
    unsigned cfg1, unsigned cfg2, unsigned cfg3)
{
    /*--------------------------------------
     * Configuration register 0
     */
    printf ("    DEVCFG0 = %08x\n", cfg0);
    if ((~cfg0 & MX1_CFG0_DEBUG_MASK) == MX1_CFG0_DEBUG_ENABLED)
        printf ("                     %u Debugger enabled\n",
            cfg0 & MX1_CFG0_DEBUG_MASK);
    else
        printf ("                     %u Debugger disabled\n",
            cfg0 & MX1_CFG0_DEBUG_MASK);

    if (~cfg0 & MX1_CFG0_JTAG_DISABLE)
        printf ("                     %u JTAG disabled\n",
            cfg0 & MX1_CFG0_JTAG_DISABLE);

    switch (~cfg0 & MX1_CFG0_ICESEL_MASK) {
    case MX1_CFG0_ICESEL_PAIR1:
        printf ("                    %02x Use PGC1/PGD1\n", cfg0 & MX1_CFG0_ICESEL_MASK);
        break;
    case MX1_CFG0_ICESEL_PAIR2:
        printf ("                    %02x Use PGC2/PGD2\n", cfg0 & MX1_CFG0_ICESEL_MASK);
        break;
    case MX1_CFG0_ICESEL_PAIR3:
        printf ("                    %02x Use PGC3/PGD3\n", cfg0 & MX1_CFG0_ICESEL_MASK);
        break;
    case MX1_CFG0_ICESEL_PAIR4:
        printf ("                    %02x Use PGC4/PGD4\n", cfg0 & MX1_CFG0_ICESEL_MASK);
        break;
    }

    if (~cfg0 & MX1_CFG0_PWP_MASK)
        printf ("                 %05x Program flash write protect\n",
            cfg0 & MX1_CFG0_PWP_MASK);

    if (~cfg0 & MX1_CFG0_BWP)
        printf ("                       Boot flash write protect\n");
    if (~cfg0 & MX1_CFG0_CP)
        printf ("                       Code protect\n");

    /*--------------------------------------
     * Configuration register 1
     */
    printf ("    DEVCFG1 = %08x\n", cfg1);
    switch (cfg1 & MX1_CFG1_FNOSC_MASK) {
    case MX1_CFG1_FNOSC_FRC:
        printf ("                     %u Fast RC oscillator\n", MX1_CFG1_FNOSC_FRC);
        break;
    case MX1_CFG1_FNOSC_FRCDIVPLL:
        printf ("                     %u Fast RC oscillator with divide-by-N and PLL\n", MX1_CFG1_FNOSC_FRCDIVPLL);
        break;
    case MX1_CFG1_FNOSC_PRI:
        printf ("                     %u Primary oscillator\n", MX1_CFG1_FNOSC_PRI);
        break;
    case MX1_CFG1_FNOSC_PRIPLL:
        printf ("                     %u Primary oscillator with PLL\n", MX1_CFG1_FNOSC_PRIPLL);
        break;
    case MX1_CFG1_FNOSC_SEC:
        printf ("                     %u Secondary oscillator\n", MX1_CFG1_FNOSC_SEC);
        break;
    case MX1_CFG1_FNOSC_LPRC:
        printf ("                     %u Low-power RC oscillator\n", MX1_CFG1_FNOSC_LPRC);
        break;
    case MX1_CFG1_FNOSC_FRCDIV16:
        printf ("                     %u Fast RC oscillator with divide-by-16\n", MX1_CFG1_FNOSC_FRCDIV16);
        break;
    case MX1_CFG1_FNOSC_FRCDIV:
        printf ("                     %u Fast RC oscillator with divide-by-N\n", MX1_CFG1_FNOSC_FRCDIV);
        break;
    default:
        printf ("                     %u UNKNOWN\n", cfg1 & MX1_CFG1_FNOSC_MASK);
        break;
    }
    if (cfg1 & MX1_CFG1_FSOSCEN)
        printf ("                    %u  Secondary oscillator enabled\n",
            MX1_CFG1_FSOSCEN >> 4);
    if (cfg1 & MX1_CFG1_IESO)
        printf ("                    %u  Internal-external switch over enabled\n",
            MX1_CFG1_IESO >> 4);

    switch (cfg1 & MX1_CFG1_POSCMOD_MASK) {
    case MX1_CFG1_POSCMOD_EXT:
        printf ("                   %u   Primary oscillator: External\n", MX1_CFG1_POSCMOD_EXT >> 8);
        break;
    case MX1_CFG1_POSCMOD_XT:
        printf ("                   %u   Primary oscillator: XT\n", MX1_CFG1_POSCMOD_XT >> 8);
        break;
    case MX1_CFG1_POSCMOD_HS:
        printf ("                   %u   Primary oscillator: HS\n", MX1_CFG1_POSCMOD_HS >> 8);
        break;
    case MX1_CFG1_POSCMOD_DISABLE:
        printf ("                   %u   Primary oscillator: disabled\n", MX1_CFG1_POSCMOD_DISABLE >> 8);
        break;
    }
    if (cfg1 & MX1_CFG1_CLKO_DISABLE)
        printf ("                   %u   CLKO output disabled\n",
            MX1_CFG1_CLKO_DISABLE >> 8);

    switch (cfg1 & MX1_CFG1_FPBDIV_MASK) {
    case MX1_CFG1_FPBDIV_1:
        printf ("                  %u    Peripheral bus clock: SYSCLK / 1\n", MX1_CFG1_FPBDIV_1 >> 12);
        break;
    case MX1_CFG1_FPBDIV_2:
        printf ("                  %u    Peripheral bus clock: SYSCLK / 2\n", MX1_CFG1_FPBDIV_2 >> 12);
        break;
    case MX1_CFG1_FPBDIV_4:
        printf ("                  %u    Peripheral bus clock: SYSCLK / 4\n", MX1_CFG1_FPBDIV_4 >> 12);
        break;
    case MX1_CFG1_FPBDIV_8:
        printf ("                  %u    Peripheral bus clock: SYSCLK / 8\n", MX1_CFG1_FPBDIV_8 >> 12);
        break;
    }
    if (cfg1 & MX1_CFG1_FCKM_ENABLE)
        printf ("                  %u    Fail-safe clock monitor enabled\n",
            MX1_CFG1_FCKM_ENABLE >> 12);
    if (cfg1 & MX1_CFG1_FCKS_ENABLE)
        printf ("                  %u    Clock switching enabled\n",
            MX1_CFG1_FCKS_ENABLE >> 12);

    if (cfg1 & MX1_CFG1_FWDTEN) {
        switch (cfg1 & MX1_CFG1_WDTPS_MASK) {
        case MX1_CFG1_WDTPS_1:
            printf ("                %2x     Watchdog postscale: 1/1\n", MX1_CFG1_WDTPS_1 >> 16);
            break;
        case MX1_CFG1_WDTPS_2:
            printf ("                %2x     Watchdog postscale: 1/2\n", MX1_CFG1_WDTPS_2 >> 16);
            break;
        case MX1_CFG1_WDTPS_4:
            printf ("                %2x     Watchdog postscale: 1/4\n", MX1_CFG1_WDTPS_4 >> 16);
            break;
        case MX1_CFG1_WDTPS_8:
            printf ("                %2x     Watchdog postscale: 1/8\n", MX1_CFG1_WDTPS_8 >> 16);
            break;
        case MX1_CFG1_WDTPS_16:
            printf ("                %2x     Watchdog postscale: 1/16\n", MX1_CFG1_WDTPS_16 >> 16);
            break;
        case MX1_CFG1_WDTPS_32:
            printf ("                %2x     Watchdog postscale: 1/32\n", MX1_CFG1_WDTPS_32 >> 16);
            break;
        case MX1_CFG1_WDTPS_64:
            printf ("                %2x     Watchdog postscale: 1/64\n", MX1_CFG1_WDTPS_64 >> 16);
            break;
        case MX1_CFG1_WDTPS_128:
            printf ("                %2x     Watchdog postscale: 1/128\n", MX1_CFG1_WDTPS_128 >> 16);
            break;
        case MX1_CFG1_WDTPS_256:
            printf ("                %2x     Watchdog postscale: 1/256\n", MX1_CFG1_WDTPS_256 >> 16);
            break;
        case MX1_CFG1_WDTPS_512:
            printf ("                %2x     Watchdog postscale: 1/512\n", MX1_CFG1_WDTPS_512 >> 16);
            break;
        case MX1_CFG1_WDTPS_1024:
            printf ("                %2x     Watchdog postscale: 1/1024\n", MX1_CFG1_WDTPS_1024 >> 16);
            break;
        case MX1_CFG1_WDTPS_2048:
            printf ("                %2x     Watchdog postscale: 1/2048\n", MX1_CFG1_WDTPS_2048 >> 16);
            break;
        case MX1_CFG1_WDTPS_4096:
            printf ("                %2x     Watchdog postscale: 1/4096\n", MX1_CFG1_WDTPS_4096 >> 16);
            break;
        case MX1_CFG1_WDTPS_8192:
            printf ("                %2x     Watchdog postscale: 1/8192\n", MX1_CFG1_WDTPS_8192 >> 16);
            break;
        case MX1_CFG1_WDTPS_16384:
            printf ("                %2x     Watchdog postscale: 1/16384\n", MX1_CFG1_WDTPS_16384 >> 16);
            break;
        case MX1_CFG1_WDTPS_32768:
            printf ("                %2x     Watchdog postscale: 1/32768\n", MX1_CFG1_WDTPS_32768 >> 16);
            break;
        case MX1_CFG1_WDTPS_65536:
            printf ("                %2x     Watchdog postscale: 1/65536\n", MX1_CFG1_WDTPS_65536 >> 16);
            break;
        case MX1_CFG1_WDTPS_131072:
            printf ("                %2x     Watchdog postscale: 1/131072\n", MX1_CFG1_WDTPS_131072 >> 16);
            break;
        case MX1_CFG1_WDTPS_262144:
            printf ("                %2x     Watchdog postscale: 1/262144\n", MX1_CFG1_WDTPS_262144 >> 16);
            break;
        case MX1_CFG1_WDTPS_524288:
            printf ("                %2x     Watchdog postscale: 1/524288\n", MX1_CFG1_WDTPS_524288 >> 16);
            break;
        case MX1_CFG1_WDTPS_1048576:
            printf ("                %2x     Watchdog postscale: 1/1048576\n", MX1_CFG1_WDTPS_1048576 >> 16);
            break;
        }

        if (cfg1 & MX1_CFG1_WINDIS)
            printf ("                %u      Watchdog in non-Window mode\n",
                MX1_CFG1_WINDIS >> 20);

        printf ("                %u      Watchdog enable\n",
            MX1_CFG1_FWDTEN >> 20);
    }

    /*--------------------------------------
     * Configuration register 2
     */
    printf ("    DEVCFG2 = %08x\n", cfg2);
    switch (cfg2 & MX1_CFG2_FPLLIDIV_MASK) {
    case MX1_CFG2_FPLLIDIV_1:
        printf ("                     %u PLL divider: 1/1\n", MX1_CFG2_FPLLIDIV_1);
        break;
    case MX1_CFG2_FPLLIDIV_2:
        printf ("                     %u PLL divider: 1/2\n", MX1_CFG2_FPLLIDIV_2);
        break;
    case MX1_CFG2_FPLLIDIV_3:
        printf ("                     %u PLL divider: 1/3\n", MX1_CFG2_FPLLIDIV_3);
        break;
    case MX1_CFG2_FPLLIDIV_4:
        printf ("                     %u PLL divider: 1/4\n", MX1_CFG2_FPLLIDIV_4);
        break;
    case MX1_CFG2_FPLLIDIV_5:
        printf ("                     %u PLL divider: 1/5\n", MX1_CFG2_FPLLIDIV_5);
        break;
    case MX1_CFG2_FPLLIDIV_6:
        printf ("                     %u PLL divider: 1/6\n", MX1_CFG2_FPLLIDIV_6);
        break;
    case MX1_CFG2_FPLLIDIV_10:
        printf ("                     %u PLL divider: 1/10\n", MX1_CFG2_FPLLIDIV_10);
        break;
    case MX1_CFG2_FPLLIDIV_12:
        printf ("                     %u PLL divider: 1/12\n", MX1_CFG2_FPLLIDIV_12);
        break;
    }
    switch (cfg2 & MX1_CFG2_FPLLMUL_MASK) {
    case MX1_CFG2_FPLLMUL_15:
        printf ("                    %u  PLL multiplier: 15x\n", MX1_CFG2_FPLLMUL_15 >> 4);
        break;
    case MX1_CFG2_FPLLMUL_16:
        printf ("                    %u  PLL multiplier: 16x\n", MX1_CFG2_FPLLMUL_16 >> 4);
        break;
    case MX1_CFG2_FPLLMUL_17:
        printf ("                    %u  PLL multiplier: 17x\n", MX1_CFG2_FPLLMUL_17 >> 4);
        break;
    case MX1_CFG2_FPLLMUL_18:
        printf ("                    %u  PLL multiplier: 18x\n", MX1_CFG2_FPLLMUL_18 >> 4);
        break;
    case MX1_CFG2_FPLLMUL_19:
        printf ("                    %u  PLL multiplier: 19x\n", MX1_CFG2_FPLLMUL_19 >> 4);
        break;
    case MX1_CFG2_FPLLMUL_20:
        printf ("                    %u  PLL multiplier: 20x\n", MX1_CFG2_FPLLMUL_20 >> 4);
        break;
    case MX1_CFG2_FPLLMUL_21:
        printf ("                    %u  PLL multiplier: 21x\n", MX1_CFG2_FPLLMUL_21 >> 4);
        break;
    case MX1_CFG2_FPLLMUL_24:
        printf ("                    %u  PLL multiplier: 24x\n", MX1_CFG2_FPLLMUL_24 >> 4);
        break;
    }
    switch (cfg2 & MX1_CFG2_UPLLIDIV_MASK) {
    case MX1_CFG2_UPLLIDIV_1:
        printf ("                   %u   USB PLL divider: 1/1\n", MX1_CFG2_UPLLIDIV_1 >> 8);
        break;
    case MX1_CFG2_UPLLIDIV_2:
        printf ("                   %u   USB PLL divider: 1/2\n", MX1_CFG2_UPLLIDIV_2 >> 8);
        break;
    case MX1_CFG2_UPLLIDIV_3:
        printf ("                   %u   USB PLL divider: 1/3\n", MX1_CFG2_UPLLIDIV_3 >> 8);
        break;
    case MX1_CFG2_UPLLIDIV_4:
        printf ("                   %u   USB PLL divider: 1/4\n", MX1_CFG2_UPLLIDIV_4 >> 8);
        break;
    case MX1_CFG2_UPLLIDIV_5:
        printf ("                   %u   USB PLL divider: 1/5\n", MX1_CFG2_UPLLIDIV_5 >> 8);
        break;
    case MX1_CFG2_UPLLIDIV_6:
        printf ("                   %u   USB PLL divider: 1/6\n", MX1_CFG2_UPLLIDIV_6 >> 8);
        break;
    case MX1_CFG2_UPLLIDIV_10:
        printf ("                   %u   USB PLL divider: 1/10\n", MX1_CFG2_UPLLIDIV_10 >> 8);
        break;
    case MX1_CFG2_UPLLIDIV_12:
        printf ("                   %u   USB PLL divider: 1/12\n", MX1_CFG2_UPLLIDIV_12 >> 8);
        break;
    }
    if (cfg2 & MX1_CFG2_UPLL_DISABLE)
        printf ("                  %u    Disable USB PLL\n",
            MX1_CFG2_UPLL_DISABLE >> 12);
    else
        printf ("                       Enable USB PLL\n");

    switch (cfg2 & MX1_CFG2_FPLLODIV_MASK) {
    case MX1_CFG2_FPLLODIV_1:
        printf ("                 %u     PLL postscaler: 1/1\n", MX1_CFG2_FPLLODIV_1 >> 16);
        break;
    case MX1_CFG2_FPLLODIV_2:
        printf ("                 %u     PLL postscaler: 1/2\n", MX1_CFG2_FPLLODIV_2 >> 16);
        break;
    case MX1_CFG2_FPLLODIV_4:
        printf ("                 %u     PLL postscaler: 1/4\n", MX1_CFG2_FPLLODIV_4 >> 16);
        break;
    case MX1_CFG2_FPLLODIV_8:
        printf ("                 %u     PLL postscaler: 1/8\n", MX1_CFG2_FPLLODIV_8 >> 16);
        break;
    case MX1_CFG2_FPLLODIV_16:
        printf ("                 %u     PLL postscaler: 1/16\n", MX1_CFG2_FPLLODIV_16 >> 16);
        break;
    case MX1_CFG2_FPLLODIV_32:
        printf ("                 %u     PLL postscaler: 1/32\n", MX1_CFG2_FPLLODIV_32 >> 16);
        break;
    case MX1_CFG2_FPLLODIV_64:
        printf ("                 %u     PLL postscaler: 1/64\n", MX1_CFG2_FPLLODIV_64 >> 16);
        break;
    case MX1_CFG2_FPLLODIV_256:
        printf ("                 %u     PLL postscaler: 1/128\n", MX1_CFG2_FPLLODIV_256 >> 16);
        break;
    }

    /*--------------------------------------
     * Configuration register 3
     */
    printf ("    DEVCFG3 = %08x\n", cfg3);
    if (~cfg3 & MX1_CFG3_USERID_MASK)
        printf ("                  %04x User-defined ID\n",
            cfg3 & MX1_CFG3_USERID_MASK);

    if (cfg3 & MX1_CFG3_PMDL1WAY)
        printf ("              %u        Peripheral Module Disable - only 1 reconfig\n",
            MX1_CFG3_PMDL1WAY >> 28);
    else
        printf ("                       USBID pin: controlled by port\n");

    if (cfg3 & MX1_CFG3_IOL1WAY)
        printf ("              %u        Peripheral Pin Select - only 1 reconfig\n",
            MX1_CFG3_IOL1WAY >> 28);
    else
        printf ("                       USBID pin: controlled by port\n");

    if (cfg3 & MX1_CFG3_FUSBIDIO)
        printf ("              %u        USBID pin: controlled by USB\n",
            MX1_CFG3_FUSBIDIO >> 28);
    else
        printf ("                       USBID pin: controlled by port\n");

    if (cfg3 & MX1_CFG3_FVBUSONIO)
        printf ("              %u        VBuson pin: controlled by USB\n",
            MX1_CFG3_FVBUSONIO >> 28);
    else
        printf ("                       VBuson pin: controlled by port\n");
}

/*
 * Print configuration for MZ family.
 */
static void print_mz (target_t *t, unsigned cfg0,
    unsigned cfg1, unsigned cfg2, unsigned cfg3)
{
    /*--------------------------------------
     * Configuration register 0
     */
    printf ("    DEVCFG0 = %08x\n", cfg0);
#if 0
    // TODO
    if ((~cfg0 & MZ_CFG0_DEBUG_MASK) == MZ_CFG0_DEBUG_ENABLED)
        printf ("                     %u Debugger enabled\n",
            cfg0 & MZ_CFG0_DEBUG_MASK);
    else
        printf ("                     %u Debugger disabled\n",
            cfg0 & MZ_CFG0_DEBUG_MASK);

    if (~cfg0 & MZ_CFG0_JTAG_DISABLE)
        printf ("                     %u JTAG disabled\n",
            cfg0 & MZ_CFG0_JTAG_DISABLE);

    switch (~cfg0 & MZ_CFG0_ICESEL_MASK) {
    case MZ_CFG0_ICESEL_PAIR1:
        printf ("                    %02x Use PGC1/PGD1\n", cfg0 & MZ_CFG0_ICESEL_MASK);
        break;
    case MZ_CFG0_ICESEL_PAIR2:
        printf ("                    %02x Use PGC2/PGD2\n", cfg0 & MZ_CFG0_ICESEL_MASK);
        break;
    case MZ_CFG0_ICESEL_PAIR3:
        printf ("                    %02x Use PGC3/PGD3\n", cfg0 & MZ_CFG0_ICESEL_MASK);
        break;
    case MZ_CFG0_ICESEL_PAIR4:
        printf ("                    %02x Use PGC4/PGD4\n", cfg0 & MZ_CFG0_ICESEL_MASK);
        break;
    }

    if (~cfg0 & MZ_CFG0_PWP_MASK)
        printf ("                 %05x Program flash write protect\n",
            cfg0 & MZ_CFG0_PWP_MASK);

    if (~cfg0 & MZ_CFG0_BWP)
        printf ("                       Boot flash write protect\n");
    if (~cfg0 & MZ_CFG0_CP)
        printf ("                       Code protect\n");
#endif

    /*--------------------------------------
     * Configuration register 1
     */
    printf ("    DEVCFG1 = %08x\n", cfg1);
#if 0
    // TODO
    switch (cfg1 & MZ_CFG1_FNOSC_MASK) {
    case MZ_CFG1_FNOSC_FRC:
        printf ("                     %u Fast RC oscillator\n", MZ_CFG1_FNOSC_FRC);
        break;
    case MZ_CFG1_FNOSC_FRCDIVPLL:
        printf ("                     %u Fast RC oscillator with divide-by-N and PLL\n", MZ_CFG1_FNOSC_FRCDIVPLL);
        break;
    case MZ_CFG1_FNOSC_PRI:
        printf ("                     %u Primary oscillator\n", MZ_CFG1_FNOSC_PRI);
        break;
    case MZ_CFG1_FNOSC_PRIPLL:
        printf ("                     %u Primary oscillator with PLL\n", MZ_CFG1_FNOSC_PRIPLL);
        break;
    case MZ_CFG1_FNOSC_SEC:
        printf ("                     %u Secondary oscillator\n", MZ_CFG1_FNOSC_SEC);
        break;
    case MZ_CFG1_FNOSC_LPRC:
        printf ("                     %u Low-power RC oscillator\n", MZ_CFG1_FNOSC_LPRC);
        break;
    case MZ_CFG1_FNOSC_FRCDIV16:
        printf ("                     %u Fast RC oscillator with divide-by-16\n", MZ_CFG1_FNOSC_FRCDIV16);
        break;
    case MZ_CFG1_FNOSC_FRCDIV:
        printf ("                     %u Fast RC oscillator with divide-by-N\n", MZ_CFG1_FNOSC_FRCDIV);
        break;
    default:
        printf ("                     %u UNKNOWN\n", cfg1 & MZ_CFG1_FNOSC_MASK);
        break;
    }
    if (cfg1 & MZ_CFG1_FSOSCEN)
        printf ("                    %u  Secondary oscillator enabled\n",
            MZ_CFG1_FSOSCEN >> 4);
    if (cfg1 & MZ_CFG1_IESO)
        printf ("                    %u  Internal-external switch over enabled\n",
            MZ_CFG1_IESO >> 4);

    switch (cfg1 & MZ_CFG1_POSCMOD_MASK) {
    case MZ_CFG1_POSCMOD_EXT:
        printf ("                   %u   Primary oscillator: External\n", MZ_CFG1_POSCMOD_EXT >> 8);
        break;
    case MZ_CFG1_POSCMOD_XT:
        printf ("                   %u   Primary oscillator: XT\n", MZ_CFG1_POSCMOD_XT >> 8);
        break;
    case MZ_CFG1_POSCMOD_HS:
        printf ("                   %u   Primary oscillator: HS\n", MZ_CFG1_POSCMOD_HS >> 8);
        break;
    case MZ_CFG1_POSCMOD_DISABLE:
        printf ("                   %u   Primary oscillator: disabled\n", MZ_CFG1_POSCMOD_DISABLE >> 8);
        break;
    }
    if (cfg1 & MZ_CFG1_CLKO_DISABLE)
        printf ("                   %u   CLKO output disabled\n",
            MZ_CFG1_CLKO_DISABLE >> 8);

    switch (cfg1 & MZ_CFG1_FPBDIV_MASK) {
    case MZ_CFG1_FPBDIV_1:
        printf ("                  %u    Peripheral bus clock: SYSCLK / 1\n", MZ_CFG1_FPBDIV_1 >> 12);
        break;
    case MZ_CFG1_FPBDIV_2:
        printf ("                  %u    Peripheral bus clock: SYSCLK / 2\n", MZ_CFG1_FPBDIV_2 >> 12);
        break;
    case MZ_CFG1_FPBDIV_4:
        printf ("                  %u    Peripheral bus clock: SYSCLK / 4\n", MZ_CFG1_FPBDIV_4 >> 12);
        break;
    case MZ_CFG1_FPBDIV_8:
        printf ("                  %u    Peripheral bus clock: SYSCLK / 8\n", MZ_CFG1_FPBDIV_8 >> 12);
        break;
    }
    if (cfg1 & MZ_CFG1_FCKM_ENABLE)
        printf ("                  %u    Fail-safe clock monitor enabled\n",
            MZ_CFG1_FCKM_ENABLE >> 12);
    if (cfg1 & MZ_CFG1_FCKS_ENABLE)
        printf ("                  %u    Clock switching enabled\n",
            MZ_CFG1_FCKS_ENABLE >> 12);

    if (cfg1 & MZ_CFG1_FWDTEN) {
        switch (cfg1 & MZ_CFG1_WDTPS_MASK) {
        case MZ_CFG1_WDTPS_1:
            printf ("                %2x     Watchdog postscale: 1/1\n", MZ_CFG1_WDTPS_1 >> 16);
            break;
        case MZ_CFG1_WDTPS_2:
            printf ("                %2x     Watchdog postscale: 1/2\n", MZ_CFG1_WDTPS_2 >> 16);
            break;
        case MZ_CFG1_WDTPS_4:
            printf ("                %2x     Watchdog postscale: 1/4\n", MZ_CFG1_WDTPS_4 >> 16);
            break;
        case MZ_CFG1_WDTPS_8:
            printf ("                %2x     Watchdog postscale: 1/8\n", MZ_CFG1_WDTPS_8 >> 16);
            break;
        case MZ_CFG1_WDTPS_16:
            printf ("                %2x     Watchdog postscale: 1/16\n", MZ_CFG1_WDTPS_16 >> 16);
            break;
        case MZ_CFG1_WDTPS_32:
            printf ("                %2x     Watchdog postscale: 1/32\n", MZ_CFG1_WDTPS_32 >> 16);
            break;
        case MZ_CFG1_WDTPS_64:
            printf ("                %2x     Watchdog postscale: 1/64\n", MZ_CFG1_WDTPS_64 >> 16);
            break;
        case MZ_CFG1_WDTPS_128:
            printf ("                %2x     Watchdog postscale: 1/128\n", MZ_CFG1_WDTPS_128 >> 16);
            break;
        case MZ_CFG1_WDTPS_256:
            printf ("                %2x     Watchdog postscale: 1/256\n", MZ_CFG1_WDTPS_256 >> 16);
            break;
        case MZ_CFG1_WDTPS_512:
            printf ("                %2x     Watchdog postscale: 1/512\n", MZ_CFG1_WDTPS_512 >> 16);
            break;
        case MZ_CFG1_WDTPS_1024:
            printf ("                %2x     Watchdog postscale: 1/1024\n", MZ_CFG1_WDTPS_1024 >> 16);
            break;
        case MZ_CFG1_WDTPS_2048:
            printf ("                %2x     Watchdog postscale: 1/2048\n", MZ_CFG1_WDTPS_2048 >> 16);
            break;
        case MZ_CFG1_WDTPS_4096:
            printf ("                %2x     Watchdog postscale: 1/4096\n", MZ_CFG1_WDTPS_4096 >> 16);
            break;
        case MZ_CFG1_WDTPS_8192:
            printf ("                %2x     Watchdog postscale: 1/8192\n", MZ_CFG1_WDTPS_8192 >> 16);
            break;
        case MZ_CFG1_WDTPS_16384:
            printf ("                %2x     Watchdog postscale: 1/16384\n", MZ_CFG1_WDTPS_16384 >> 16);
            break;
        case MZ_CFG1_WDTPS_32768:
            printf ("                %2x     Watchdog postscale: 1/32768\n", MZ_CFG1_WDTPS_32768 >> 16);
            break;
        case MZ_CFG1_WDTPS_65536:
            printf ("                %2x     Watchdog postscale: 1/65536\n", MZ_CFG1_WDTPS_65536 >> 16);
            break;
        case MZ_CFG1_WDTPS_131072:
            printf ("                %2x     Watchdog postscale: 1/131072\n", MZ_CFG1_WDTPS_131072 >> 16);
            break;
        case MZ_CFG1_WDTPS_262144:
            printf ("                %2x     Watchdog postscale: 1/262144\n", MZ_CFG1_WDTPS_262144 >> 16);
            break;
        case MZ_CFG1_WDTPS_524288:
            printf ("                %2x     Watchdog postscale: 1/524288\n", MZ_CFG1_WDTPS_524288 >> 16);
            break;
        case MZ_CFG1_WDTPS_1048576:
            printf ("                %2x     Watchdog postscale: 1/1048576\n", MZ_CFG1_WDTPS_1048576 >> 16);
            break;
        }

        if (cfg1 & MZ_CFG1_WINDIS)
            printf ("                %u      Watchdog in non-Window mode\n",
                MZ_CFG1_WINDIS >> 20);

        printf ("                %u      Watchdog enable\n",
            MZ_CFG1_FWDTEN >> 20);
    }
#endif

    /*--------------------------------------
     * Configuration register 2
     */
    printf ("    DEVCFG2 = %08x\n", cfg2);
#if 0
    // TODO
    switch (cfg2 & MZ_CFG2_FPLLIDIV_MASK) {
    case MZ_CFG2_FPLLIDIV_1:
        printf ("                     %u PLL divider: 1/1\n", MZ_CFG2_FPLLIDIV_1);
        break;
    case MZ_CFG2_FPLLIDIV_2:
        printf ("                     %u PLL divider: 1/2\n", MZ_CFG2_FPLLIDIV_2);
        break;
    case MZ_CFG2_FPLLIDIV_3:
        printf ("                     %u PLL divider: 1/3\n", MZ_CFG2_FPLLIDIV_3);
        break;
    case MZ_CFG2_FPLLIDIV_4:
        printf ("                     %u PLL divider: 1/4\n", MZ_CFG2_FPLLIDIV_4);
        break;
    case MZ_CFG2_FPLLIDIV_5:
        printf ("                     %u PLL divider: 1/5\n", MZ_CFG2_FPLLIDIV_5);
        break;
    case MZ_CFG2_FPLLIDIV_6:
        printf ("                     %u PLL divider: 1/6\n", MZ_CFG2_FPLLIDIV_6);
        break;
    case MZ_CFG2_FPLLIDIV_10:
        printf ("                     %u PLL divider: 1/10\n", MZ_CFG2_FPLLIDIV_10);
        break;
    case MZ_CFG2_FPLLIDIV_12:
        printf ("                     %u PLL divider: 1/12\n", MZ_CFG2_FPLLIDIV_12);
        break;
    }
    switch (cfg2 & MZ_CFG2_FPLLMUL_MASK) {
    case MZ_CFG2_FPLLMUL_15:
        printf ("                    %u  PLL multiplier: 15x\n", MZ_CFG2_FPLLMUL_15 >> 4);
        break;
    case MZ_CFG2_FPLLMUL_16:
        printf ("                    %u  PLL multiplier: 16x\n", MZ_CFG2_FPLLMUL_16 >> 4);
        break;
    case MZ_CFG2_FPLLMUL_17:
        printf ("                    %u  PLL multiplier: 17x\n", MZ_CFG2_FPLLMUL_17 >> 4);
        break;
    case MZ_CFG2_FPLLMUL_18:
        printf ("                    %u  PLL multiplier: 18x\n", MZ_CFG2_FPLLMUL_18 >> 4);
        break;
    case MZ_CFG2_FPLLMUL_19:
        printf ("                    %u  PLL multiplier: 19x\n", MZ_CFG2_FPLLMUL_19 >> 4);
        break;
    case MZ_CFG2_FPLLMUL_20:
        printf ("                    %u  PLL multiplier: 20x\n", MZ_CFG2_FPLLMUL_20 >> 4);
        break;
    case MZ_CFG2_FPLLMUL_21:
        printf ("                    %u  PLL multiplier: 21x\n", MZ_CFG2_FPLLMUL_21 >> 4);
        break;
    case MZ_CFG2_FPLLMUL_24:
        printf ("                    %u  PLL multiplier: 24x\n", MZ_CFG2_FPLLMUL_24 >> 4);
        break;
    }
    switch (cfg2 & MZ_CFG2_UPLLIDIV_MASK) {
    case MZ_CFG2_UPLLIDIV_1:
        printf ("                   %u   USB PLL divider: 1/1\n", MZ_CFG2_UPLLIDIV_1 >> 8);
        break;
    case MZ_CFG2_UPLLIDIV_2:
        printf ("                   %u   USB PLL divider: 1/2\n", MZ_CFG2_UPLLIDIV_2 >> 8);
        break;
    case MZ_CFG2_UPLLIDIV_3:
        printf ("                   %u   USB PLL divider: 1/3\n", MZ_CFG2_UPLLIDIV_3 >> 8);
        break;
    case MZ_CFG2_UPLLIDIV_4:
        printf ("                   %u   USB PLL divider: 1/4\n", MZ_CFG2_UPLLIDIV_4 >> 8);
        break;
    case MZ_CFG2_UPLLIDIV_5:
        printf ("                   %u   USB PLL divider: 1/5\n", MZ_CFG2_UPLLIDIV_5 >> 8);
        break;
    case MZ_CFG2_UPLLIDIV_6:
        printf ("                   %u   USB PLL divider: 1/6\n", MZ_CFG2_UPLLIDIV_6 >> 8);
        break;
    case MZ_CFG2_UPLLIDIV_10:
        printf ("                   %u   USB PLL divider: 1/10\n", MZ_CFG2_UPLLIDIV_10 >> 8);
        break;
    case MZ_CFG2_UPLLIDIV_12:
        printf ("                   %u   USB PLL divider: 1/12\n", MZ_CFG2_UPLLIDIV_12 >> 8);
        break;
    }
    if (cfg2 & MZ_CFG2_UPLL_DISABLE)
        printf ("                  %u    Disable USB PLL\n",
            MZ_CFG2_UPLL_DISABLE >> 12);
    else
        printf ("                       Enable USB PLL\n");

    switch (cfg2 & MZ_CFG2_FPLLODIV_MASK) {
    case MZ_CFG2_FPLLODIV_1:
        printf ("                 %u     PLL postscaler: 1/1\n", MZ_CFG2_FPLLODIV_1 >> 16);
        break;
    case MZ_CFG2_FPLLODIV_2:
        printf ("                 %u     PLL postscaler: 1/2\n", MZ_CFG2_FPLLODIV_2 >> 16);
        break;
    case MZ_CFG2_FPLLODIV_4:
        printf ("                 %u     PLL postscaler: 1/4\n", MZ_CFG2_FPLLODIV_4 >> 16);
        break;
    case MZ_CFG2_FPLLODIV_8:
        printf ("                 %u     PLL postscaler: 1/8\n", MZ_CFG2_FPLLODIV_8 >> 16);
        break;
    case MZ_CFG2_FPLLODIV_16:
        printf ("                 %u     PLL postscaler: 1/16\n", MZ_CFG2_FPLLODIV_16 >> 16);
        break;
    case MZ_CFG2_FPLLODIV_32:
        printf ("                 %u     PLL postscaler: 1/32\n", MZ_CFG2_FPLLODIV_32 >> 16);
        break;
    case MZ_CFG2_FPLLODIV_64:
        printf ("                 %u     PLL postscaler: 1/64\n", MZ_CFG2_FPLLODIV_64 >> 16);
        break;
    case MZ_CFG2_FPLLODIV_256:
        printf ("                 %u     PLL postscaler: 1/128\n", MZ_CFG2_FPLLODIV_256 >> 16);
        break;
    }
#endif

    /*--------------------------------------
     * Configuration register 3
     */
    printf ("    DEVCFG3 = %08x\n", cfg3);
#if 0
    // TODO
    if (~cfg3 & MZ_CFG3_USERID_MASK)
        printf ("                  %04x User-defined ID\n",
            cfg3 & MZ_CFG3_USERID_MASK);

    if (cfg3 & MZ_CFG3_PMDL1WAY)
        printf ("              %u        Peripheral Module Disable - only 1 reconfig\n",
            MZ_CFG3_PMDL1WAY >> 28);
    else
        printf ("                       USBID pin: controlled by port\n");

    if (cfg3 & MZ_CFG3_IOL1WAY)
        printf ("              %u        Peripheral Pin Select - only 1 reconfig\n",
            MZ_CFG3_IOL1WAY >> 28);
    else
        printf ("                       USBID pin: controlled by port\n");

    if (cfg3 & MZ_CFG3_FUSBIDIO)
        printf ("              %u        USBID pin: controlled by USB\n",
            MZ_CFG3_FUSBIDIO >> 28);
    else
        printf ("                       USBID pin: controlled by port\n");

    if (cfg3 & MZ_CFG3_FVBUSONIO)
        printf ("              %u        VBuson pin: controlled by USB\n",
            MZ_CFG3_FVBUSONIO >> 28);
    else
        printf ("                       VBuson pin: controlled by port\n");
#endif
}

/*
 * Print configuration registers of the target CPU.
 */
void target_print_devcfg (target_t *t)
{
    unsigned devcfg_addr = 0x1fc00000 + target_devcfg_offset (t);
    unsigned devcfg3 = t->adapter->read_word (t->adapter, devcfg_addr);
    unsigned devcfg2 = t->adapter->read_word (t->adapter, devcfg_addr + 4);
    unsigned devcfg1 = t->adapter->read_word (t->adapter, devcfg_addr + 8);
    unsigned devcfg0 = t->adapter->read_word (t->adapter, devcfg_addr + 12);

    if (devcfg3 == 0xffffffff && devcfg2 == 0xffffffff &&
        devcfg1 == 0xffffffff && devcfg0 == 0x7fffffff)
        return;
    if (devcfg3 == 0 && devcfg2 == 0 && devcfg1 == 0 && devcfg0 == 0)
        return;

    printf (_("Configuration:\n"));
    t->family->print_devcfg (t, devcfg0, devcfg1, devcfg2, devcfg3);
}

/*
 * Translate virtual to physical address.
 */
static unsigned virt_to_phys (unsigned addr)
{
    if (addr >= 0x80000000 && addr < 0xA0000000)
        return addr - 0x80000000;
    if (addr >= 0xA0000000 && addr < 0xC0000000)
        return addr - 0xA0000000;
    return addr;
}

/*
 * Read data from memory.
 */
void target_read_block (target_t *t, unsigned addr,
    unsigned nwords, unsigned *data)
{
    if (! t->adapter->read_data) {
        printf (_("\nData reading not supported by the adapter.\n"));
        exit (1);
    }

    addr = virt_to_phys (addr);
    //fprintf (stderr, "target_read_block (addr = %x, nwords = %d)\n", addr, nwords);
    while (nwords > 0) {
        unsigned n = nwords;
        if (n > 256)
            n = 256;
        t->adapter->read_data (t->adapter, addr, n, data);
        addr += n<<2;
        data += n;
        nwords -= n;
    }
    //fprintf (stderr, "    done (addr = %x)\n", addr);
}

/*
 * Verify data.
 */
void target_verify_block (target_t *t, unsigned addr,
    unsigned nwords, unsigned *data)
{
    unsigned i, word, expected, block[512];

//fprintf (stderr, "%s: addr=%08x, nwords=%u, data=%08x...\n", __func__, addr, nwords, data[0]);
    if (t->adapter->verify_data != 0) {
        t->adapter->verify_data (t->adapter, virt_to_phys (addr), nwords, data);
        return;
    }

    t->adapter->read_data (t->adapter, addr, nwords, block);
    for (i=0; i<nwords; i++) {
        expected = data [i];
        word = block [i];
        if (word != expected) {
            printf (_("\nerror at address %08X: file=%08X, mem=%08X\n"),
                addr + i*4, expected, word);
            exit (1);
        }
    }
}

/*
 * Erase all Flash memory.
 */
int target_erase (target_t *t)
{
    printf (_("        Erase: "));
    fflush (stdout);
    t->adapter->erase_chip (t->adapter);
    printf (_("done\n"));
    return 1;
}

/*
 * Test block for non 0xFFFFFFFF value
 */
static int target_test_empty_block (unsigned *data, unsigned nwords)
{
    while (nwords--)
        if (*data++ != 0xFFFFFFFF)
            return 0;
    return 1;
}

/*
 * Write to flash memory.
 */
void target_program_block (target_t *t, unsigned addr,
    unsigned nwords, unsigned *data)
{
    addr = virt_to_phys (addr);
    //fprintf (stderr, "target_program_block (addr = %x, nwords = %d)\n", addr, nwords);

    if (! t->adapter->program_block) {
        unsigned words_per_row = t->family->bytes_per_row / 4;
        while (nwords > 0) {
            unsigned n = nwords;
            if (n > words_per_row)
                n = words_per_row;
	    if (! target_test_empty_block (data, words_per_row))
                t->adapter->program_row (t->adapter, addr, data, words_per_row);
            addr += n<<2;
            data += n;
            nwords -= n;
        }
    }
    while (nwords > 0) {
        unsigned n = nwords;
        if (n > 256)
            n = 256;
        t->adapter->program_block (t->adapter, addr, data);
        addr += n<<2;
        data += n;
        nwords -= n;
    }
}

/*
 * Program the configuration registers.
 */
void target_program_devcfg (target_t *t, unsigned devcfg0,
        unsigned devcfg1, unsigned devcfg2, unsigned devcfg3)
{
    unsigned addr = 0x1fc00000 + t->family->devcfg_offset;

//fprintf (stderr, "%s: devcfg0-3 = %08x %08x %08x %08x\n", __func__, devcfg0, devcfg1, devcfg2, devcfg3);
    if (t->family->pe_version >= 0x0500) {
        /* Since pic32mz, the programming executive */
        t->adapter->program_quad_word (t->adapter, addr, devcfg3,
            devcfg2, devcfg1, devcfg0);

//unsigned cfg3 = t->adapter->read_word (t->adapter, addr);
//unsigned cfg2 = t->adapter->read_word (t->adapter, addr + 4);
//unsigned cfg1 = t->adapter->read_word (t->adapter, addr + 8);
//unsigned cfg0 = t->adapter->read_word (t->adapter, addr + 12);
        return;
    }

    t->adapter->program_word (t->adapter, addr, devcfg3);
    t->adapter->program_word (t->adapter, addr + 4, devcfg2);
    t->adapter->program_word (t->adapter, addr + 8, devcfg1);
    t->adapter->program_word (t->adapter, addr + 12, devcfg0);
//unsigned cfg3 = t->adapter->read_word (t->adapter, addr);
//unsigned cfg2 = t->adapter->read_word (t->adapter, addr + 4);
//unsigned cfg1 = t->adapter->read_word (t->adapter, addr + 8);
//unsigned cfg0 = t->adapter->read_word (t->adapter, addr + 12);
}
