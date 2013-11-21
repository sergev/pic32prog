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

struct _target_t {
    adapter_t   *adapter;
    const char  *cpu_name;
    unsigned    cpuid;
    unsigned    flash_addr;
    unsigned    flash_bytes;
    unsigned    boot_bytes;
    unsigned    pe;
};

static const struct {
    unsigned devid;
    const char *name;
    unsigned flash_kbytes;
    unsigned boot_kbytes;
    unsigned pe;
} pic32mx_dev[] = {
    /* MX1/2 family-------------Flash-Boot-PE */
    {0x4A07053, "MX110F016B",     16,  3,  1},
    {0x4A09053, "MX110F016C",     16,  3,  1},
    {0x4A0B053, "MX110F016D",     16,  3,  1},
    {0x4A06053, "MX120F032B",     32,  3,  1},
    {0x4A08053, "MX120F032C",     32,  3,  1},
    {0x4A0A053, "MX120F032D",     32,  3,  1},
    {0x4D07053, "MX130F064B",     64,  3,  1},
    {0x4D09053, "MX130F064C",     64,  3,  1},
    {0x4D0B053, "MX130F064D",     64,  3,  1},
    {0x4D06053, "MX150F128B",    128,  3,  1},
    {0x4D08053, "MX150F128C",    128,  3,  1},
    {0x4D0A053, "MX150F128D",    128,  3,  1},
    {0x4A01053, "MX210F016B",     16,  3,  1},
    {0x4A03053, "MX210F016C",     16,  3,  1},
    {0x4A05053, "MX210F016D",     16,  3,  1},
    {0x4A00053, "MX220F032B",     32,  3,  1},
    {0x4A02053, "MX220F032C",     32,  3,  1},
    {0x4A04053, "MX220F032D",     32,  3,  1},
    {0x4D01053, "MX230F064B",     64,  3,  1},
    {0x4D03053, "MX230F064C",     64,  3,  1},
    {0x4D05053, "MX230F064D",     64,  3,  1},
    {0x4D00053, "MX250F128B",    128,  3,  1},
    {0x4D02053, "MX250F128C",    128,  3,  1},
    {0x4D04053, "MX250F128D",    128,  3,  1},

    /* MX3/4/5/6/7 family-------Flash-Boot-PE */
    {0x0902053, "MX320F032H",     32, 12,  0},
    {0x0906053, "MX320F064H",     64, 12,  0},
    {0x090A053, "MX320F128H",    128, 12,  0},
    {0x092A053, "MX320F128L",    128, 12,  0},
    {0x5600053, "MX330F064H",     64, 12,  0},
    {0x5601053, "MX330F064L",     64, 12,  0},
    {0x090D053, "MX340F128H",    128, 12,  0},
    {0x092D053, "MX340F128L",    128, 12,  0},
    {0x0912053, "MX340F256H",    256, 12,  0},
    {0x0916053, "MX340F512H",    512, 12,  0},
    {0x570C053, "MX350F128H",    128, 12,  0},
    {0x570D053, "MX350F128L",    128, 12,  0},
    {0x5704053, "MX350F256H",    256, 12,  0},
    {0x5705053, "MX350F256L",    256, 12,  0},
    {0x0934053, "MX360F256L",    256, 12,  0},
    {0x0938053, "MX360F512L",    512, 12,  0},
    {0x5808053, "MX370F512H",    512, 12,  0},
    {0x5809053, "MX370F512L",    512, 12,  0},
    {0x0942053, "MX420F032H",     32, 12,  0},
    {0x5602053, "MX430F064H",     64, 12,  0},
    {0x5603053, "MX430F064L",     64, 12,  0},
    {0x094D053, "MX440F128H",    128, 12,  0},
    {0x096D053, "MX440F128L",    128, 12,  0},
    {0x0952053, "MX440F256H",    256, 12,  0},
    {0x0956053, "MX440F512H",    512, 12,  0},
    {0x570E053, "MX450F128H",    128, 12,  0},
    {0x570F053, "MX450F128L",    128, 12,  0},
    {0x5706053, "MX450F256H",    256, 12,  0},
    {0x5707053, "MX450F256L",    256, 12,  0},
    {0x0974053, "MX460F256L",    256, 12,  0},
    {0x0978053, "MX460F512L",    512, 12,  0},
    {0x580A053, "MX470F512H",    512, 12,  0},
    {0x580B053, "MX470F512L",    512, 12,  0},
    {0x4400053, "MX534F064H",     64, 12,  0},
    {0x440C053, "MX534F064L",     64, 12,  0},
    {0x4401053, "MX564F064H",     64, 12,  0},
    {0x440D053, "MX564F064L",     64, 12,  0},
    {0x4403053, "MX564F128H",    128, 12,  0},
    {0x440F053, "MX564F128L",    128, 12,  0},
    {0x4317053, "MX575F256H",    256, 12,  0},
    {0x4333053, "MX575F256L",    256, 12,  0},
    {0x4309053, "MX575F512H",    512, 12,  0},
    {0x430F053, "MX575F512L",    512, 12,  0},
    {0x4405053, "MX664F064H",     64, 12,  0},
    {0x4411053, "MX664F064L",     64, 12,  0},
    {0x4407053, "MX664F128H",    128, 12,  0},
    {0x4413053, "MX664F128L",    128, 12,  0},
    {0x430B053, "MX675F256H",    256, 12,  0},
    {0x4305053, "MX675F256L",    256, 12,  0},
    {0x430C053, "MX675F512H",    512, 12,  0},
    {0x4311053, "MX675F512L",    512, 12,  0},
    {0x4325053, "MX695F512H",    512, 12,  0},
    {0x4341053, "MX695F512L",    512, 12,  0},
    {0x440B053, "MX764F128H",    128, 12,  0},
    {0x4417053, "MX764F128L",    128, 12,  0},
    {0x4303053, "MX775F256H",    256, 12,  0},
    {0x4312053, "MX775F256L",    256, 12,  0},
    {0x430D053, "MX775F512H",    512, 12,  0},
    {0x4306053, "MX775F512L",    512, 12,  0},
    {0x430E053, "MX795F512H",    512, 12,  0},
    {0x4307053, "MX795F512L",    512, 12,  0},

    /* MZ family----------------Flash-Boot-PE */
    {0x5100053, "MZ0256ECE064",  256, 160, 2},
    {0x510A053, "MZ0256ECE100",  256, 160, 2},
    {0x5114053, "MZ0256ECE124",  256, 160, 2},
    {0x511E053, "MZ0256ECE144",  256, 160, 2},
    {0x5105053, "MZ0256ECF064",  256, 160, 2},
    {0x510F053, "MZ0256ECF100",  256, 160, 2},
    {0x5119053, "MZ0256ECF124",  256, 160, 2},
    {0x5123053, "MZ0256ECF144",  256, 160, 2},
    {0x5101053, "MZ0512ECE064",  512, 160, 2},
    {0x510B053, "MZ0512ECE100",  512, 160, 2},
    {0x5115053, "MZ0512ECE124",  512, 160, 2},
    {0x511F053, "MZ0512ECE144",  512, 160, 2},
    {0x5106053, "MZ0512ECF064",  512, 160, 2},
    {0x5110053, "MZ0512ECF100",  512, 160, 2},
    {0x511A053, "MZ0512ECF124",  512, 160, 2},
    {0x5124053, "MZ0512ECF144",  512, 160, 2},
    {0x5102053, "MZ1024ECE064", 1024, 160, 2},
    {0x510C053, "MZ1024ECE100", 1024, 160, 2},
    {0x5116053, "MZ1024ECE124", 1024, 160, 2},
    {0x5120053, "MZ1024ECE144", 1024, 160, 2},
    {0x5107053, "MZ1024ECF064", 1024, 160, 2},
    {0x5111053, "MZ1024ECF100", 1024, 160, 2},
    {0x511B053, "MZ1024ECF124", 1024, 160, 2},
    {0x5125053, "MZ1024ECF144", 1024, 160, 2},
    {0x5103053, "MZ1024ECG064", 1024, 160, 2},
    {0x510D053, "MZ1024ECG100", 1024, 160, 2},
    {0x5117053, "MZ1024ECG124", 1024, 160, 2},
    {0x5121053, "MZ1024ECG144", 1024, 160, 2},
    {0x5108053, "MZ1024ECH064", 1024, 160, 2},
    {0x5112053, "MZ1024ECH100", 1024, 160, 2},
    {0x511C053, "MZ1024ECH124", 1024, 160, 2},
    {0x5126053, "MZ1024ECH144", 1024, 160, 2},
    {0x5104053, "MZ2048ECG064", 2048, 160, 2},
    {0x510E053, "MZ2048ECG100", 2048, 160, 2},
    {0x5118053, "MZ2048ECG124", 2048, 160, 2},
    {0x5122053, "MZ2048ECG144", 2048, 160, 2},
    {0x5109053, "MZ2048ECH064", 2048, 160, 2},
    {0x5113053, "MZ2048ECH100", 2048, 160, 2},
    {0x511D053, "MZ2048ECH124", 2048, 160, 2},
    {0x5127053, "MZ2048ECH144", 2048, 160, 2},

    {0xEAFB00B, "Bootloader",   0,    0,   0},  /* USB bootloader */
    {0}
};

static const struct {
    const unsigned *code;
    unsigned nwords;
    unsigned version;
} pic32_pe[] = {
   /*-Code----------Nwords--Version-*/
    { pic32_pemx3,  1044,   0x0201 },   /* mx3/4/5/6/7 */
    { pic32_pemx1,  422,    0x0301 },   /* mx1/mx2 */
    { pic32_pemz,   1052,   0x0502 },   /* mz */
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
    t->cpu_name = pic32mx_dev[i].name;
    t->flash_addr = 0x1d000000;
    t->flash_bytes = pic32mx_dev[i].flash_kbytes * 1024;
    t->boot_bytes = pic32mx_dev[i].boot_kbytes * 1024;
    t->pe = pic32mx_dev[i].pe;
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
    return t->boot_bytes;
}

/*
 * Use PE for reading/writing/erasing memory.
 */
void target_use_executable (target_t *t)
{
    if (t->adapter->load_executable != 0)
        t->adapter->load_executable (t->adapter, pic32_pe[t->pe].code,
            pic32_pe[t->pe].nwords, pic32_pe[t->pe].version);
}

void target_print_devcfg (target_t *t)
{
    unsigned devcfg3 = t->adapter->read_word (t->adapter, 0x1fc00000 + t->boot_bytes - 16);
    unsigned devcfg2 = t->adapter->read_word (t->adapter, 0x1fc00000 + t->boot_bytes - 12);
    unsigned devcfg1 = t->adapter->read_word (t->adapter, 0x1fc00000 + t->boot_bytes - 8);
    unsigned devcfg0 = t->adapter->read_word (t->adapter, 0x1fc00000 + t->boot_bytes - 4);
    if (devcfg3 == 0xffffffff && devcfg2 == 0xffffffff &&
        devcfg1 == 0xffffffff && devcfg0 == 0x7fffffff)
        return;
    if (devcfg3 == 0 && devcfg2 == 0 && devcfg1 == 0 && devcfg0 == 0)
        return;

    printf (_("Configuration:\n"));

    /*--------------------------------------
     * Configuration register 0
     */
    printf ("    DEVCFG0 = %08x\n", devcfg0);
    if ((~devcfg0 & DEVCFG0_DEBUG_MASK) == DEVCFG0_DEBUG_ENABLED)
        printf ("                     %u Debugger enabled\n",
            devcfg0 & DEVCFG0_DEBUG_MASK);
    else
        printf ("                     %u Debugger disabled\n",
            devcfg0 & DEVCFG0_DEBUG_MASK);

    switch (~devcfg0 & DEVCFG0_ICESEL_MASK) {
    case DEVCFG0_ICESEL_PAIR1:
        printf ("                    %02x Use PGC1/PGD1\n", devcfg0 & DEVCFG0_ICESEL_MASK);
        break;
    case DEVCFG0_ICESEL_PAIR2:
        printf ("                    %02x Use PGC2/PGD2\n", devcfg0 & DEVCFG0_ICESEL_MASK);
        break;
    case DEVCFG0_ICESEL_PAIR3:
        printf ("                    %02x Use PGC3/PGD3\n", devcfg0 & DEVCFG0_ICESEL_MASK);
        break;
    case DEVCFG0_ICESEL_PAIR4:
        printf ("                    %02x Use PGC4/PGD4\n", devcfg0 & DEVCFG0_ICESEL_MASK);
        break;
    }

    if (~devcfg0 & DEVCFG0_PWP_MASK)
        printf ("                 %05x Program flash write protect\n",
            devcfg0 & DEVCFG0_PWP_MASK);

    if (~devcfg0 & DEVCFG0_BWP)
        printf ("                       Boot flash write protect\n");
    if (~devcfg0 & DEVCFG0_CP)
        printf ("                       Code protect\n");

    /*--------------------------------------
     * Configuration register 1
     */
    printf ("    DEVCFG1 = %08x\n", devcfg1);
    switch (devcfg1 & DEVCFG1_FNOSC_MASK) {
    case DEVCFG1_FNOSC_FRC:
        printf ("                     %u Fast RC oscillator\n", DEVCFG1_FNOSC_FRC);
        break;
    case DEVCFG1_FNOSC_FRCDIVPLL:
        printf ("                     %u Fast RC oscillator with divide-by-N and PLL\n", DEVCFG1_FNOSC_FRCDIVPLL);
        break;
    case DEVCFG1_FNOSC_PRI:
        printf ("                     %u Primary oscillator\n", DEVCFG1_FNOSC_PRI);
        break;
    case DEVCFG1_FNOSC_PRIPLL:
        printf ("                     %u Primary oscillator with PLL\n", DEVCFG1_FNOSC_PRIPLL);
        break;
    case DEVCFG1_FNOSC_SEC:
        printf ("                     %u Secondary oscillator\n", DEVCFG1_FNOSC_SEC);
        break;
    case DEVCFG1_FNOSC_LPRC:
        printf ("                     %u Low-power RC oscillator\n", DEVCFG1_FNOSC_LPRC);
        break;
    case DEVCFG1_FNOSC_FRCDIV:
        printf ("                     %u Fast RC oscillator with divide-by-N\n", DEVCFG1_FNOSC_FRCDIV);
        break;
    default:
        printf ("                     %u UNKNOWN\n", devcfg1 & DEVCFG1_FNOSC_MASK);
        break;
    }
    if (devcfg1 & DEVCFG1_FSOSCEN)
        printf ("                    %u  Secondary oscillator enabled\n",
            DEVCFG1_FSOSCEN >> 4);
    if (devcfg1 & DEVCFG1_IESO)
        printf ("                    %u  Internal-external switch over enabled\n",
            DEVCFG1_IESO >> 4);

    switch (devcfg1 & DEVCFG1_POSCMOD_MASK) {
    case DEVCFG1_POSCMOD_EXT:
        printf ("                   %u   Primary oscillator: External\n", DEVCFG1_POSCMOD_EXT >> 8);
        break;
    case DEVCFG1_POSCMOD_XT:
        printf ("                   %u   Primary oscillator: XT\n", DEVCFG1_POSCMOD_XT >> 8);
        break;
    case DEVCFG1_POSCMOD_HS:
        printf ("                   %u   Primary oscillator: HS\n", DEVCFG1_POSCMOD_HS >> 8);
        break;
    case DEVCFG1_POSCMOD_DISABLE:
        printf ("                   %u   Primary oscillator: disabled\n", DEVCFG1_POSCMOD_DISABLE >> 8);
        break;
    }
    if (! (devcfg1 & DEVCFG1_OSCIOFNC))
        printf ("                   %u   CLKO output active\n",
            DEVCFG1_OSCIOFNC >> 8);

    switch (devcfg1 & DEVCFG1_FPBDIV_MASK) {
    case DEVCFG1_FPBDIV_1:
        printf ("                  %u    Peripheral bus clock: SYSCLK / 1\n", DEVCFG1_FPBDIV_1 >> 12);
        break;
    case DEVCFG1_FPBDIV_2:
        printf ("                  %u    Peripheral bus clock: SYSCLK / 2\n", DEVCFG1_FPBDIV_2 >> 12);
        break;
    case DEVCFG1_FPBDIV_4:
        printf ("                  %u    Peripheral bus clock: SYSCLK / 4\n", DEVCFG1_FPBDIV_4 >> 12);
        break;
    case DEVCFG1_FPBDIV_8:
        printf ("                  %u    Peripheral bus clock: SYSCLK / 8\n", DEVCFG1_FPBDIV_8 >> 12);
        break;
    }
    if (devcfg1 & DEVCFG1_FCKM_DISABLE)
        printf ("                  %u    Fail-safe clock monitor disable\n",
            DEVCFG1_FCKM_DISABLE >> 12);
    if (devcfg1 & DEVCFG1_FCKS_DISABLE)
        printf ("                  %u    Clock switching disable\n",
            DEVCFG1_FCKS_DISABLE >> 12);

    switch (devcfg1 & DEVCFG1_WDTPS_MASK) {
    case DEVCFG1_WDTPS_1:
        printf ("                %2x     Watchdog postscale: 1/1\n", DEVCFG1_WDTPS_1 >> 16);
        break;
    case DEVCFG1_WDTPS_2:
        printf ("                %2x     Watchdog postscale: 1/2\n", DEVCFG1_WDTPS_2 >> 16);
        break;
    case DEVCFG1_WDTPS_4:
        printf ("                %2x     Watchdog postscale: 1/4\n", DEVCFG1_WDTPS_4 >> 16);
        break;
    case DEVCFG1_WDTPS_8:
        printf ("                %2x     Watchdog postscale: 1/8\n", DEVCFG1_WDTPS_8 >> 16);
        break;
    case DEVCFG1_WDTPS_16:
        printf ("                %2x     Watchdog postscale: 1/16\n", DEVCFG1_WDTPS_16 >> 16);
        break;
    case DEVCFG1_WDTPS_32:
        printf ("                %2x     Watchdog postscale: 1/32\n", DEVCFG1_WDTPS_32 >> 16);
        break;
    case DEVCFG1_WDTPS_64:
        printf ("                %2x     Watchdog postscale: 1/64\n", DEVCFG1_WDTPS_64 >> 16);
        break;
    case DEVCFG1_WDTPS_128:
        printf ("                %2x     Watchdog postscale: 1/128\n", DEVCFG1_WDTPS_128 >> 16);
        break;
    case DEVCFG1_WDTPS_256:
        printf ("                %2x     Watchdog postscale: 1/256\n", DEVCFG1_WDTPS_256 >> 16);
        break;
    case DEVCFG1_WDTPS_512:
        printf ("                %2x     Watchdog postscale: 1/512\n", DEVCFG1_WDTPS_512 >> 16);
        break;
    case DEVCFG1_WDTPS_1024:
        printf ("                %2x     Watchdog postscale: 1/1024\n", DEVCFG1_WDTPS_1024 >> 16);
        break;
    case DEVCFG1_WDTPS_2048:
        printf ("                %2x     Watchdog postscale: 1/2048\n", DEVCFG1_WDTPS_2048 >> 16);
        break;
    case DEVCFG1_WDTPS_4096:
        printf ("                %2x     Watchdog postscale: 1/4096\n", DEVCFG1_WDTPS_4096 >> 16);
        break;
    case DEVCFG1_WDTPS_8192:
        printf ("                %2x     Watchdog postscale: 1/8192\n", DEVCFG1_WDTPS_8192 >> 16);
        break;
    case DEVCFG1_WDTPS_16384:
        printf ("                %2x     Watchdog postscale: 1/16384\n", DEVCFG1_WDTPS_16384 >> 16);
        break;
    case DEVCFG1_WDTPS_32768:
        printf ("                %2x     Watchdog postscale: 1/32768\n", DEVCFG1_WDTPS_32768 >> 16);
        break;
    case DEVCFG1_WDTPS_65536:
        printf ("                %2x     Watchdog postscale: 1/65536\n", DEVCFG1_WDTPS_65536 >> 16);
        break;
    case DEVCFG1_WDTPS_131072:
        printf ("                %2x     Watchdog postscale: 1/131072\n", DEVCFG1_WDTPS_131072 >> 16);
        break;
    case DEVCFG1_WDTPS_262144:
        printf ("                %2x     Watchdog postscale: 1/262144\n", DEVCFG1_WDTPS_262144 >> 16);
        break;
    case DEVCFG1_WDTPS_524288:
        printf ("                %2x     Watchdog postscale: 1/524288\n", DEVCFG1_WDTPS_524288 >> 16);
        break;
    case DEVCFG1_WDTPS_1048576:
        printf ("                %2x     Watchdog postscale: 1/1048576\n", DEVCFG1_WDTPS_1048576 >> 16);
        break;
    }
    if (devcfg1 & DEVCFG1_FWDTEN)
        printf ("                %u      Watchdog enable\n",
            DEVCFG1_FWDTEN >> 20);

    /*--------------------------------------
     * Configuration register 2
     */
    printf ("    DEVCFG2 = %08x\n", devcfg2);
    switch (devcfg2 & DEVCFG2_FPLLIDIV_MASK) {
    case DEVCFG2_FPLLIDIV_1:
        printf ("                     %u PLL divider: 1/1\n", DEVCFG2_FPLLIDIV_1);
        break;
    case DEVCFG2_FPLLIDIV_2:
        printf ("                     %u PLL divider: 1/2\n", DEVCFG2_FPLLIDIV_2);
        break;
    case DEVCFG2_FPLLIDIV_3:
        printf ("                     %u PLL divider: 1/3\n", DEVCFG2_FPLLIDIV_3);
        break;
    case DEVCFG2_FPLLIDIV_4:
        printf ("                     %u PLL divider: 1/4\n", DEVCFG2_FPLLIDIV_4);
        break;
    case DEVCFG2_FPLLIDIV_5:
        printf ("                     %u PLL divider: 1/5\n", DEVCFG2_FPLLIDIV_5);
        break;
    case DEVCFG2_FPLLIDIV_6:
        printf ("                     %u PLL divider: 1/6\n", DEVCFG2_FPLLIDIV_6);
        break;
    case DEVCFG2_FPLLIDIV_10:
        printf ("                     %u PLL divider: 1/10\n", DEVCFG2_FPLLIDIV_10);
        break;
    case DEVCFG2_FPLLIDIV_12:
        printf ("                     %u PLL divider: 1/12\n", DEVCFG2_FPLLIDIV_12);
        break;
    }
    switch (devcfg2 & DEVCFG2_FPLLMUL_MASK) {
    case DEVCFG2_FPLLMUL_15:
        printf ("                    %u  PLL multiplier: 15x\n", DEVCFG2_FPLLMUL_15 >> 4);
        break;
    case DEVCFG2_FPLLMUL_16:
        printf ("                    %u  PLL multiplier: 16x\n", DEVCFG2_FPLLMUL_16 >> 4);
        break;
    case DEVCFG2_FPLLMUL_17:
        printf ("                    %u  PLL multiplier: 17x\n", DEVCFG2_FPLLMUL_17 >> 4);
        break;
    case DEVCFG2_FPLLMUL_18:
        printf ("                    %u  PLL multiplier: 18x\n", DEVCFG2_FPLLMUL_18 >> 4);
        break;
    case DEVCFG2_FPLLMUL_19:
        printf ("                    %u  PLL multiplier: 19x\n", DEVCFG2_FPLLMUL_19 >> 4);
        break;
    case DEVCFG2_FPLLMUL_20:
        printf ("                    %u  PLL multiplier: 20x\n", DEVCFG2_FPLLMUL_20 >> 4);
        break;
    case DEVCFG2_FPLLMUL_21:
        printf ("                    %u  PLL multiplier: 21x\n", DEVCFG2_FPLLMUL_21 >> 4);
        break;
    case DEVCFG2_FPLLMUL_24:
        printf ("                    %u  PLL multiplier: 24x\n", DEVCFG2_FPLLMUL_24 >> 4);
        break;
    }
    switch (devcfg2 & DEVCFG2_UPLLIDIV_MASK) {
    case DEVCFG2_UPLLIDIV_1:
        printf ("                   %u   USB PLL divider: 1/1\n", DEVCFG2_UPLLIDIV_1 >> 8);
        break;
    case DEVCFG2_UPLLIDIV_2:
        printf ("                   %u   USB PLL divider: 1/2\n", DEVCFG2_UPLLIDIV_2 >> 8);
        break;
    case DEVCFG2_UPLLIDIV_3:
        printf ("                   %u   USB PLL divider: 1/3\n", DEVCFG2_UPLLIDIV_3 >> 8);
        break;
    case DEVCFG2_UPLLIDIV_4:
        printf ("                   %u   USB PLL divider: 1/4\n", DEVCFG2_UPLLIDIV_4 >> 8);
        break;
    case DEVCFG2_UPLLIDIV_5:
        printf ("                   %u   USB PLL divider: 1/5\n", DEVCFG2_UPLLIDIV_5 >> 8);
        break;
    case DEVCFG2_UPLLIDIV_6:
        printf ("                   %u   USB PLL divider: 1/6\n", DEVCFG2_UPLLIDIV_6 >> 8);
        break;
    case DEVCFG2_UPLLIDIV_10:
        printf ("                   %u   USB PLL divider: 1/10\n", DEVCFG2_UPLLIDIV_10 >> 8);
        break;
    case DEVCFG2_UPLLIDIV_12:
        printf ("                   %u   USB PLL divider: 1/12\n", DEVCFG2_UPLLIDIV_12 >> 8);
        break;
    }
    if (devcfg2 & DEVCFG2_UPLLDIS)
        printf ("                  %u    Disable USB PLL\n",
            DEVCFG2_UPLLDIS >> 12);
    else
        printf ("                       Enable USB PLL\n");

    switch (devcfg2 & DEVCFG2_FPLLODIV_MASK) {
    case DEVCFG2_FPLLODIV_1:
        printf ("                 %u     PLL postscaler: 1/1\n", DEVCFG2_FPLLODIV_1 >> 16);
        break;
    case DEVCFG2_FPLLODIV_2:
        printf ("                 %u     PLL postscaler: 1/2\n", DEVCFG2_FPLLODIV_2 >> 16);
        break;
    case DEVCFG2_FPLLODIV_4:
        printf ("                 %u     PLL postscaler: 1/4\n", DEVCFG2_FPLLODIV_4 >> 16);
        break;
    case DEVCFG2_FPLLODIV_8:
        printf ("                 %u     PLL postscaler: 1/8\n", DEVCFG2_FPLLODIV_8 >> 16);
        break;
    case DEVCFG2_FPLLODIV_16:
        printf ("                 %u     PLL postscaler: 1/16\n", DEVCFG2_FPLLODIV_16 >> 16);
        break;
    case DEVCFG2_FPLLODIV_32:
        printf ("                 %u     PLL postscaler: 1/32\n", DEVCFG2_FPLLODIV_32 >> 16);
        break;
    case DEVCFG2_FPLLODIV_64:
        printf ("                 %u     PLL postscaler: 1/64\n", DEVCFG2_FPLLODIV_64 >> 16);
        break;
    case DEVCFG2_FPLLODIV_256:
        printf ("                 %u     PLL postscaler: 1/128\n", DEVCFG2_FPLLODIV_256 >> 16);
        break;
    }

    /*--------------------------------------
     * Configuration register 3
     */
    printf ("    DEVCFG3 = %08x\n", devcfg3);
    if (~devcfg3 & DEVCFG3_USERID_MASK)
        printf ("                  %04x User-defined ID\n",
            devcfg3 & DEVCFG3_USERID_MASK);

    switch (devcfg3 & DEVCFG3_FSRSSEL_MASK) {
    case DEVCFG3_FSRSSEL_ALL:
        printf ("                 %u     All irqs assigned to shadow set\n", DEVCFG3_FSRSSEL_ALL >> 16);
        break;
    case DEVCFG3_FSRSSEL_1:
        printf ("                 %u     Assign irq priority 1 to shadow set\n", DEVCFG3_FSRSSEL_1 >> 16);
        break;
    case DEVCFG3_FSRSSEL_2:
        printf ("                 %u     Assign irq priority 2 to shadow set\n", DEVCFG3_FSRSSEL_2 >> 16);
        break;
    case DEVCFG3_FSRSSEL_3:
        printf ("                 %u     Assign irq priority 3 to shadow set\n", DEVCFG3_FSRSSEL_3 >> 16);
        break;
    case DEVCFG3_FSRSSEL_4:
        printf ("                 %u     Assign irq priority 4 to shadow set\n", DEVCFG3_FSRSSEL_4 >> 16);
        break;
    case DEVCFG3_FSRSSEL_5:
        printf ("                 %u     Assign irq priority 5 to shadow set\n", DEVCFG3_FSRSSEL_5 >> 16);
        break;
    case DEVCFG3_FSRSSEL_6:
        printf ("                 %u     Assign irq priority 6 to shadow set\n", DEVCFG3_FSRSSEL_6 >> 16);
        break;
    case DEVCFG3_FSRSSEL_7:
        printf ("                 %u     Assign irq priority 7 to shadow set\n", DEVCFG3_FSRSSEL_7 >> 16);
        break;
    }
    if (devcfg3 & DEVCFG3_FMIIEN)
        printf ("               %u       Ethernet MII enabled\n",
            DEVCFG3_FMIIEN >> 24);
    else
        printf ("                       Ethernet RMII enabled\n");

    if (devcfg3 & DEVCFG3_FETHIO)
        printf ("               %u       Default Ethernet i/o pins\n",
            DEVCFG3_FETHIO >> 24);
    else
        printf ("                       Alternate Ethernet i/o pins\n");

    if (devcfg3 & DEVCFG3_FCANIO)
        printf ("               %u       Default CAN i/o pins\n",
            DEVCFG3_FCANIO >> 24);
    else
        printf ("                       Alternate CAN i/o pins\n");

    if (devcfg3 & DEVCFG3_FUSBIDIO)
        printf ("              %u        USBID pin: controlled by USB\n",
            DEVCFG3_FUSBIDIO >> 28);
    else
        printf ("                       USBID pin: controlled by port\n");

    if (devcfg3 & DEVCFG3_FVBUSONIO)
        printf ("              %u        VBuson pin: controlled by USB\n",
            DEVCFG3_FVBUSONIO >> 28);
    else
        printf ("                       VBuson pin: controlled by port\n");
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
        t->adapter->read_data (t->adapter, addr, 256, data);
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
    unsigned i, word, expected, block[256];

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
        if (target_boot_bytes (t) <= 3*1024) {
            while (nwords > 0) {
                unsigned n = nwords;
                if (n > 32)
                    n = 32;
		if (! target_test_empty_block (data, 32))
                    t->adapter->program_row32 (t->adapter, addr, data);
                addr += n<<2;
                data += n;
                nwords -= n;
            }
            return;
        }
        while (nwords > 0) {
            unsigned n = nwords;
            if (n > 128)
                n = 128;
	    if (! target_test_empty_block (data, 128))
                t->adapter->program_row128 (t->adapter, addr, data);
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
 * Write one word.
 */
void target_program_word (target_t *t, unsigned addr, unsigned word)
{
    addr = virt_to_phys (addr);
    //fprintf (stderr, "target_program_word (%08x) = %08x\n", addr, word);
    t->adapter->program_word (t->adapter, addr, word);
}
