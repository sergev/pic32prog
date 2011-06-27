/*
 * Interface to PIC32 microcontroller via debug port.
 *
 * Copyright (C) 2011 Serge Vakulenko
 *
 * This file is part of BKUNIX project, which is distributed
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
};

static const struct {
    unsigned devid;
    const char *name;
    unsigned bytes;
} pic32mx_dev[] = {
    {0x04A07053, "110F016B",  16*1024},
    {0x04A09053, "110F016C",  16*1024},
    {0x04A0B053, "110F016D",  16*1024},
    {0x04A06053, "120F032B",  32*1024},
    {0x04A08053, "120F032C",  32*1024},
    {0x04A0A053, "120F032D",  32*1024},
    {0x04A01053, "210F016B",  16*1024},
    {0x04A03053, "210F016C",  16*1024},
    {0x04A05053, "210F016D",  16*1024},
    {0x04A00053, "220F032B",  32*1024},
    {0x04A02053, "220F032C",  32*1024},
    {0x04A04053, "220F032D",  32*1024},
    {0x00938053, "360F512L", 512*1024},
    {0x00934053, "360F256L", 256*1024},
    {0x0092D053, "340F128L", 128*1024},
    {0x0092A053, "320F128L", 128*1024},
    {0x00916053, "340F512H", 512*1024},
    {0x00912053, "340F256H", 256*1024},
    {0x0090D053, "340F128H", 128*1024},
    {0x0090A053, "320F128H", 128*1024},
    {0x00906053, "320F064H",  64*1024},
    {0x00902053, "320F032H",  32*1024},
    {0x00978053, "460F512L", 512*1024},
    {0x00974053, "460F256L", 256*1024},
    {0x0096D053, "440F128L", 128*1024},
    {0x00952053, "440F256H", 256*1024},
    {0x00956053, "440F512H", 512*1024},
    {0x0094D053, "440F128H", 128*1024},
    {0x00942053, "420F032H",  32*1024},
    {0x04307053, "795F512L", 512*1024},
    {0x0430E053, "795F512H", 512*1024},
    {0x04306053, "775F512L", 512*1024},
    {0x0430D053, "775F512H", 512*1024},
    {0x04312053, "775F256L", 256*1024},
    {0x04303053, "775F256H", 256*1024},
    {0x04417053, "764F128L", 128*1024},
    {0x0440B053, "764F128H", 128*1024},
    {0x04341053, "695F512L", 512*1024},
    {0x04325053, "695F512H", 512*1024},
    {0x04311053, "675F512L", 512*1024},
    {0x0430C053, "675F512H", 512*1024},
    {0x04305053, "675F256L", 256*1024},
    {0x0430B053, "675F256H", 256*1024},
    {0x04413053, "664F128L", 128*1024},
    {0x04407053, "664F128H", 128*1024},
    {0x04411053, "664F064L",  64*1024},
    {0x04405053, "664F064H",  64*1024},
    {0x0430F053, "575F512L", 512*1024},
    {0x04309053, "575F512H", 512*1024},
    {0x04333053, "575F256L", 256*1024},
    {0x04317053, "575F256H", 256*1024},
    {0x0440F053, "564F128L", 128*1024},
    {0x04403053, "564F128H", 128*1024},
    {0x0440D053, "564F064L",  64*1024},
    {0x04401053, "564F064H",  64*1024},
    {0x04400053, "534F064H",  64*1024},
    {0x0440C053, "534F064L",  64*1024},
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
    t->adapter = adapter_open_pickit2 ();
    if (! t->adapter)
        t->adapter = adapter_open_mpsse ();
    if (! t->adapter) {
        fprintf (stderr, _("No target found.\n"));
        exit (-1);
    }

    /* Check CPU identifier. */
    t->cpuid = t->adapter->get_idcode (t->adapter);
    unsigned i;
    for (i=0; t->cpuid != pic32mx_dev[i].devid; i++) {
        if (pic32mx_dev[i].devid == 0) {
            /* Device not detected. */
            fprintf (stderr, _("Unknown CPUID=%08x.\n"), t->cpuid);
            t->adapter->close (t->adapter, 0);
            exit (1);
        }
    }
    t->cpu_name = pic32mx_dev[i].name;
    t->flash_addr = 0x1d000000;
    t->flash_bytes = pic32mx_dev[i].bytes;
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

/*
 * Use PE for reading/writing/erasing memory.
 */
void target_use_executable (target_t *t)
{
    if (t->adapter->load_executable != 0)
        t->adapter->load_executable (t->adapter);
}

void target_print_devcfg (target_t *t)
{
    unsigned devcfg0 = t->adapter->read_word (t->adapter, DEVCFG0_ADDR);
    unsigned devcfg1 = t->adapter->read_word (t->adapter, DEVCFG1_ADDR);
    unsigned devcfg2 = t->adapter->read_word (t->adapter, DEVCFG2_ADDR);
    unsigned devcfg3 = t->adapter->read_word (t->adapter, DEVCFG3_ADDR);
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

    if (~devcfg0 & DEVCFG0_ICESEL)
        printf ("                       Use PGC1/PGD1\n");
    else
        printf ("                     %u Use PGC2/PGD2\n",
            DEVCFG0_ICESEL);

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
    if (devcfg1 & DEVCFG1_OSCIOFNC)
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
 * Write to flash memory.
 */
void target_program_block (target_t *t, unsigned addr,
    unsigned nwords, unsigned *data)
{
    addr = virt_to_phys (addr);
    //fprintf (stderr, "target_program_block (addr = %x, nwords = %d)\n", addr, nwords);
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
