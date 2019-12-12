/*
 * Routines specific for PIC32 MX3/4/5/6/7 family.
 *
 * Copyright (C) 2013 Serge Vakulenko
 *
 * This file is part of PIC32PROG project, which is distributed
 * under the terms of the GNU General Public License (GPL).
 * See the accompanying file "COPYING" for more details.
 */
#include <stdio.h>
#include "pic32.h"

/*
 * Print configuration for MX3/4/5/6/7 family.
 */
void print_mx3(unsigned cfg0, unsigned cfg1, unsigned cfg2, unsigned cfg3,
				unsigned cfg4, unsigned cfg5, unsigned cfg6, unsigned cfg7,
				unsigned cfg8, unsigned cfg9, unsigned cfg10, unsigned cfg11,
				unsigned cfg12, unsigned cfg13, unsigned cfg14, unsigned cfg15,
				unsigned cfg16, unsigned cfg17)
{
    /*--------------------------------------
     * Configuration register 0
     */
    printf("    DEVCFG0 = %08x\n", cfg0);
    if ((~cfg0 & MX3_CFG0_DEBUG_MASK) == MX3_CFG0_DEBUG_ENABLED)
        printf("                     %u Debugger enabled\n",
            cfg0 & MX3_CFG0_DEBUG_MASK);
    else
        printf("                     %u Debugger disabled\n",
            cfg0 & MX3_CFG0_DEBUG_MASK);

    if (~cfg0 & MX3_CFG0_JTAG_DISABLE)
        printf("                     %u JTAG disabled\n",
            cfg0 & MX3_CFG0_JTAG_DISABLE);

    switch (~cfg0 & MX3_CFG0_ICESEL_MASK) {
    case MX3_CFG0_ICESEL_PAIR1:
        printf("                    %02x Use PGC1/PGD1\n", cfg0 & MX3_CFG0_ICESEL_MASK);
        break;
    case MX3_CFG0_ICESEL_PAIR2:
        printf("                    %02x Use PGC2/PGD2\n", cfg0 & MX3_CFG0_ICESEL_MASK);
        break;
    case MX3_CFG0_ICESEL_PAIR3:
        printf("                    %02x Use PGC3/PGD3\n", cfg0 & MX3_CFG0_ICESEL_MASK);
        break;
    case MX3_CFG0_ICESEL_PAIR4:
        printf("                    %02x Use PGC4/PGD4\n", cfg0 & MX3_CFG0_ICESEL_MASK);
        break;
    }

    if (~cfg0 & MX3_CFG0_PWP_MASK)
        printf("                 %05x Program flash write protect\n",
            cfg0 & MX3_CFG0_PWP_MASK);

    if (~cfg0 & MX3_CFG0_BWP)
        printf("                       Boot flash write protect\n");
    if (~cfg0 & MX3_CFG0_CP)
        printf("                       Code protect\n");

    /*--------------------------------------
     * Configuration register 1
     */
    printf("    DEVCFG1 = %08x\n", cfg1);
    switch (cfg1 & MX3_CFG1_FNOSC_MASK) {
    case MX3_CFG1_FNOSC_FRC:
        printf("                     %u Fast RC oscillator\n", MX3_CFG1_FNOSC_FRC);
        break;
    case MX3_CFG1_FNOSC_FRCDIVPLL:
        printf("                     %u Fast RC oscillator with divide-by-N and PLL\n", MX3_CFG1_FNOSC_FRCDIVPLL);
        break;
    case MX3_CFG1_FNOSC_PRI:
        printf("                     %u Primary oscillator\n", MX3_CFG1_FNOSC_PRI);
        break;
    case MX3_CFG1_FNOSC_PRIPLL:
        printf("                     %u Primary oscillator with PLL\n", MX3_CFG1_FNOSC_PRIPLL);
        break;
    case MX3_CFG1_FNOSC_SEC:
        printf("                     %u Secondary oscillator\n", MX3_CFG1_FNOSC_SEC);
        break;
    case MX3_CFG1_FNOSC_LPRC:
        printf("                     %u Low-power RC oscillator\n", MX3_CFG1_FNOSC_LPRC);
        break;
    case MX3_CFG1_FNOSC_FRCDIV16:
        printf("                     %u Fast RC oscillator with divide-by-16\n", MX3_CFG1_FNOSC_FRCDIV16);
        break;
    case MX3_CFG1_FNOSC_FRCDIV:
        printf("                     %u Fast RC oscillator with divide-by-N\n", MX3_CFG1_FNOSC_FRCDIV);
        break;
    default:
        printf("                     %u UNKNOWN\n", cfg1 & MX3_CFG1_FNOSC_MASK);
        break;
    }
    if (cfg1 & MX3_CFG1_FSOSCEN)
        printf("                    %u  Secondary oscillator enabled\n",
            MX3_CFG1_FSOSCEN >> 4);
    if (cfg1 & MX3_CFG1_IESO)
        printf("                    %u  Internal-external switch over enabled\n",
            MX3_CFG1_IESO >> 4);

    switch (cfg1 & MX3_CFG1_POSCMOD_MASK) {
    case MX3_CFG1_POSCMOD_EXT:
        printf("                   %u   Primary oscillator: External\n", MX3_CFG1_POSCMOD_EXT >> 8);
        break;
    case MX3_CFG1_POSCMOD_XT:
        printf("                   %u   Primary oscillator: XT\n", MX3_CFG1_POSCMOD_XT >> 8);
        break;
    case MX3_CFG1_POSCMOD_HS:
        printf("                   %u   Primary oscillator: HS\n", MX3_CFG1_POSCMOD_HS >> 8);
        break;
    case MX3_CFG1_POSCMOD_DISABLE:
        printf("                   %u   Primary oscillator: disabled\n", MX3_CFG1_POSCMOD_DISABLE >> 8);
        break;
    }
    if (cfg1 & MX3_CFG1_CLKO_DISABLE)
        printf("                   %u   CLKO output disabled\n",
            MX3_CFG1_CLKO_DISABLE >> 8);

    switch (cfg1 & MX3_CFG1_FPBDIV_MASK) {
    case MX3_CFG1_FPBDIV_1:
        printf("                  %u    Peripheral bus clock: SYSCLK / 1\n", MX3_CFG1_FPBDIV_1 >> 12);
        break;
    case MX3_CFG1_FPBDIV_2:
        printf("                  %u    Peripheral bus clock: SYSCLK / 2\n", MX3_CFG1_FPBDIV_2 >> 12);
        break;
    case MX3_CFG1_FPBDIV_4:
        printf("                  %u    Peripheral bus clock: SYSCLK / 4\n", MX3_CFG1_FPBDIV_4 >> 12);
        break;
    case MX3_CFG1_FPBDIV_8:
        printf("                  %u    Peripheral bus clock: SYSCLK / 8\n", MX3_CFG1_FPBDIV_8 >> 12);
        break;
    }
    if (cfg1 & MX3_CFG1_FCKM_DISABLE)
        printf("                  %u    Fail-safe clock monitor disable\n",
            MX3_CFG1_FCKM_DISABLE >> 12);
    if (cfg1 & MX3_CFG1_FCKS_DISABLE)
        printf("                  %u    Clock switching disable\n",
            MX3_CFG1_FCKS_DISABLE >> 12);

    switch (cfg1 & MX3_CFG1_WDTPS_MASK) {
    case MX3_CFG1_WDTPS_1:
        printf("                %2x     Watchdog postscale: 1/1\n", MX3_CFG1_WDTPS_1 >> 16);
        break;
    case MX3_CFG1_WDTPS_2:
        printf("                %2x     Watchdog postscale: 1/2\n", MX3_CFG1_WDTPS_2 >> 16);
        break;
    case MX3_CFG1_WDTPS_4:
        printf("                %2x     Watchdog postscale: 1/4\n", MX3_CFG1_WDTPS_4 >> 16);
        break;
    case MX3_CFG1_WDTPS_8:
        printf("                %2x     Watchdog postscale: 1/8\n", MX3_CFG1_WDTPS_8 >> 16);
        break;
    case MX3_CFG1_WDTPS_16:
        printf("                %2x     Watchdog postscale: 1/16\n", MX3_CFG1_WDTPS_16 >> 16);
        break;
    case MX3_CFG1_WDTPS_32:
        printf("                %2x     Watchdog postscale: 1/32\n", MX3_CFG1_WDTPS_32 >> 16);
        break;
    case MX3_CFG1_WDTPS_64:
        printf("                %2x     Watchdog postscale: 1/64\n", MX3_CFG1_WDTPS_64 >> 16);
        break;
    case MX3_CFG1_WDTPS_128:
        printf("                %2x     Watchdog postscale: 1/128\n", MX3_CFG1_WDTPS_128 >> 16);
        break;
    case MX3_CFG1_WDTPS_256:
        printf("                %2x     Watchdog postscale: 1/256\n", MX3_CFG1_WDTPS_256 >> 16);
        break;
    case MX3_CFG1_WDTPS_512:
        printf("                %2x     Watchdog postscale: 1/512\n", MX3_CFG1_WDTPS_512 >> 16);
        break;
    case MX3_CFG1_WDTPS_1024:
        printf("                %2x     Watchdog postscale: 1/1024\n", MX3_CFG1_WDTPS_1024 >> 16);
        break;
    case MX3_CFG1_WDTPS_2048:
        printf ("                %2x     Watchdog postscale: 1/2048\n", MX3_CFG1_WDTPS_2048 >> 16);
        break;
    case MX3_CFG1_WDTPS_4096:
        printf ("                %2x     Watchdog postscale: 1/4096\n", MX3_CFG1_WDTPS_4096 >> 16);
        break;
    case MX3_CFG1_WDTPS_8192:
        printf("                %2x     Watchdog postscale: 1/8192\n", MX3_CFG1_WDTPS_8192 >> 16);
        break;
    case MX3_CFG1_WDTPS_16384:
        printf("                %2x     Watchdog postscale: 1/16384\n", MX3_CFG1_WDTPS_16384 >> 16);
        break;
    case MX3_CFG1_WDTPS_32768:
        printf("                %2x     Watchdog postscale: 1/32768\n", MX3_CFG1_WDTPS_32768 >> 16);
        break;
    case MX3_CFG1_WDTPS_65536:
        printf("                %2x     Watchdog postscale: 1/65536\n", MX3_CFG1_WDTPS_65536 >> 16);
        break;
    case MX3_CFG1_WDTPS_131072:
        printf("                %2x     Watchdog postscale: 1/131072\n", MX3_CFG1_WDTPS_131072 >> 16);
        break;
    case MX3_CFG1_WDTPS_262144:
        printf("                %2x     Watchdog postscale: 1/262144\n", MX3_CFG1_WDTPS_262144 >> 16);
        break;
    case MX3_CFG1_WDTPS_524288:
        printf("                %2x     Watchdog postscale: 1/524288\n", MX3_CFG1_WDTPS_524288 >> 16);
        break;
    case MX3_CFG1_WDTPS_1048576:
        printf("                %2x     Watchdog postscale: 1/1048576\n", MX3_CFG1_WDTPS_1048576 >> 16);
        break;
    }
    if (cfg1 & MX3_CFG1_FWDTEN)
        printf("                %u      Watchdog enable\n",
            MX3_CFG1_FWDTEN >> 20);

    /*--------------------------------------
     * Configuration register 2
     */
    printf("    DEVCFG2 = %08x\n", cfg2);
    switch (cfg2 & MX3_CFG2_FPLLIDIV_MASK) {
    case MX3_CFG2_FPLLIDIV_1:
        printf("                     %u PLL divider: 1/1\n", MX3_CFG2_FPLLIDIV_1);
        break;
    case MX3_CFG2_FPLLIDIV_2:
        printf("                     %u PLL divider: 1/2\n", MX3_CFG2_FPLLIDIV_2);
        break;
    case MX3_CFG2_FPLLIDIV_3:
        printf("                     %u PLL divider: 1/3\n", MX3_CFG2_FPLLIDIV_3);
        break;
    case MX3_CFG2_FPLLIDIV_4:
        printf("                     %u PLL divider: 1/4\n", MX3_CFG2_FPLLIDIV_4);
        break;
    case MX3_CFG2_FPLLIDIV_5:
        printf("                     %u PLL divider: 1/5\n", MX3_CFG2_FPLLIDIV_5);
        break;
    case MX3_CFG2_FPLLIDIV_6:
        printf("                     %u PLL divider: 1/6\n", MX3_CFG2_FPLLIDIV_6);
        break;
    case MX3_CFG2_FPLLIDIV_10:
        printf("                     %u PLL divider: 1/10\n", MX3_CFG2_FPLLIDIV_10);
        break;
    case MX3_CFG2_FPLLIDIV_12:
        printf("                     %u PLL divider: 1/12\n", MX3_CFG2_FPLLIDIV_12);
        break;
    }
    switch (cfg2 & MX3_CFG2_FPLLMUL_MASK) {
    case MX3_CFG2_FPLLMUL_15:
        printf("                    %u  PLL multiplier: 15x\n", MX3_CFG2_FPLLMUL_15 >> 4);
        break;
    case MX3_CFG2_FPLLMUL_16:
        printf("                    %u  PLL multiplier: 16x\n", MX3_CFG2_FPLLMUL_16 >> 4);
        break;
    case MX3_CFG2_FPLLMUL_17:
        printf("                    %u  PLL multiplier: 17x\n", MX3_CFG2_FPLLMUL_17 >> 4);
        break;
    case MX3_CFG2_FPLLMUL_18:
        printf("                    %u  PLL multiplier: 18x\n", MX3_CFG2_FPLLMUL_18 >> 4);
        break;
    case MX3_CFG2_FPLLMUL_19:
        printf("                    %u  PLL multiplier: 19x\n", MX3_CFG2_FPLLMUL_19 >> 4);
        break;
    case MX3_CFG2_FPLLMUL_20:
        printf("                    %u  PLL multiplier: 20x\n", MX3_CFG2_FPLLMUL_20 >> 4);
        break;
    case MX3_CFG2_FPLLMUL_21:
        printf("                    %u  PLL multiplier: 21x\n", MX3_CFG2_FPLLMUL_21 >> 4);
        break;
    case MX3_CFG2_FPLLMUL_24:
        printf("                    %u  PLL multiplier: 24x\n", MX3_CFG2_FPLLMUL_24 >> 4);
        break;
    }
    switch (cfg2 & MX3_CFG2_UPLLIDIV_MASK) {
    case MX3_CFG2_UPLLIDIV_1:
        printf("                   %u   USB PLL divider: 1/1\n", MX3_CFG2_UPLLIDIV_1 >> 8);
        break;
    case MX3_CFG2_UPLLIDIV_2:
        printf("                   %u   USB PLL divider: 1/2\n", MX3_CFG2_UPLLIDIV_2 >> 8);
        break;
    case MX3_CFG2_UPLLIDIV_3:
        printf("                   %u   USB PLL divider: 1/3\n", MX3_CFG2_UPLLIDIV_3 >> 8);
        break;
    case MX3_CFG2_UPLLIDIV_4:
        printf("                   %u   USB PLL divider: 1/4\n", MX3_CFG2_UPLLIDIV_4 >> 8);
        break;
    case MX3_CFG2_UPLLIDIV_5:
        printf("                   %u   USB PLL divider: 1/5\n", MX3_CFG2_UPLLIDIV_5 >> 8);
        break;
    case MX3_CFG2_UPLLIDIV_6:
        printf("                   %u   USB PLL divider: 1/6\n", MX3_CFG2_UPLLIDIV_6 >> 8);
        break;
    case MX3_CFG2_UPLLIDIV_10:
        printf("                   %u   USB PLL divider: 1/10\n", MX3_CFG2_UPLLIDIV_10 >> 8);
        break;
    case MX3_CFG2_UPLLIDIV_12:
        printf("                   %u   USB PLL divider: 1/12\n", MX3_CFG2_UPLLIDIV_12 >> 8);
        break;
    }
    if (cfg2 & MX3_CFG2_UPLL_DISABLE)
        printf("                  %u    Disable USB PLL\n",
            MX3_CFG2_UPLL_DISABLE >> 12);
    else
        printf("                       Enable USB PLL\n");

    switch (cfg2 & MX3_CFG2_FPLLODIV_MASK) {
    case MX3_CFG2_FPLLODIV_1:
        printf("                 %u     PLL postscaler: 1/1\n", MX3_CFG2_FPLLODIV_1 >> 16);
        break;
    case MX3_CFG2_FPLLODIV_2:
        printf("                 %u     PLL postscaler: 1/2\n", MX3_CFG2_FPLLODIV_2 >> 16);
        break;
    case MX3_CFG2_FPLLODIV_4:
        printf("                 %u     PLL postscaler: 1/4\n", MX3_CFG2_FPLLODIV_4 >> 16);
        break;
    case MX3_CFG2_FPLLODIV_8:
        printf("                 %u     PLL postscaler: 1/8\n", MX3_CFG2_FPLLODIV_8 >> 16);
        break;
    case MX3_CFG2_FPLLODIV_16:
        printf("                 %u     PLL postscaler: 1/16\n", MX3_CFG2_FPLLODIV_16 >> 16);
        break;
    case MX3_CFG2_FPLLODIV_32:
        printf("                 %u     PLL postscaler: 1/32\n", MX3_CFG2_FPLLODIV_32 >> 16);
        break;
    case MX3_CFG2_FPLLODIV_64:
        printf("                 %u     PLL postscaler: 1/64\n", MX3_CFG2_FPLLODIV_64 >> 16);
        break;
    case MX3_CFG2_FPLLODIV_256:
        printf("                 %u     PLL postscaler: 1/128\n", MX3_CFG2_FPLLODIV_256 >> 16);
        break;
    }

    /*--------------------------------------
     * Configuration register 3
     */
    printf("    DEVCFG3 = %08x\n", cfg3);
    if (~cfg3 & MX3_CFG3_USERID_MASK)
        printf("                  %04x User-defined ID\n",
            cfg3 & MX3_CFG3_USERID_MASK);

    switch (cfg3 & MX3_CFG3_FSRSSEL_MASK) {
    case MX3_CFG3_FSRSSEL_ALL:
        printf("                 %u     All irqs assigned to shadow set\n", MX3_CFG3_FSRSSEL_ALL >> 16);
        break;
    case MX3_CFG3_FSRSSEL_1:
        printf("                 %u     Assign irq priority 1 to shadow set\n", MX3_CFG3_FSRSSEL_1 >> 16);
        break;
    case MX3_CFG3_FSRSSEL_2:
        printf("                 %u     Assign irq priority 2 to shadow set\n", MX3_CFG3_FSRSSEL_2 >> 16);
        break;
    case MX3_CFG3_FSRSSEL_3:
        printf("                 %u     Assign irq priority 3 to shadow set\n", MX3_CFG3_FSRSSEL_3 >> 16);
        break;
    case MX3_CFG3_FSRSSEL_4:
        printf("                 %u     Assign irq priority 4 to shadow set\n", MX3_CFG3_FSRSSEL_4 >> 16);
        break;
    case MX3_CFG3_FSRSSEL_5:
        printf("                 %u     Assign irq priority 5 to shadow set\n", MX3_CFG3_FSRSSEL_5 >> 16);
        break;
    case MX3_CFG3_FSRSSEL_6:
        printf("                 %u     Assign irq priority 6 to shadow set\n", MX3_CFG3_FSRSSEL_6 >> 16);
        break;
    case MX3_CFG3_FSRSSEL_7:
        printf("                 %u     Assign irq priority 7 to shadow set\n", MX3_CFG3_FSRSSEL_7 >> 16);
        break;
    }
    if (cfg3 & MX3_CFG3_FMIIEN)
        printf("               %u       Ethernet MII enabled\n",
            MX3_CFG3_FMIIEN >> 24);
    else
        printf("                       Ethernet RMII enabled\n");

    if (cfg3 & MX3_CFG3_FETHIO)
        printf("               %u       Default Ethernet i/o pins\n",
            MX3_CFG3_FETHIO >> 24);
    else
        printf("                       Alternate Ethernet i/o pins\n");

    if (cfg3 & MX3_CFG3_FCANIO)
        printf("               %u       Default CAN i/o pins\n",
            MX3_CFG3_FCANIO >> 24);
    else
        printf("                       Alternate CAN i/o pins\n");

    if (cfg3 & MX3_CFG3_FUSBIDIO)
        printf("              %u        USBID pin: controlled by USB\n",
            MX3_CFG3_FUSBIDIO >> 28);
    else
        printf("                       USBID pin: controlled by port\n");

    if (cfg3 & MX3_CFG3_FVBUSONIO)
        printf("              %u        VBuson pin: controlled by USB\n",
            MX3_CFG3_FVBUSONIO >> 28);
    else
        printf("                       VBuson pin: controlled by port\n");
}
