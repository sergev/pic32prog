/*
 * Routines specific for PIC32 MZ family.
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
 * Print configuration for MZ family.
 */
void print_mz(unsigned cfg0, unsigned cfg1, unsigned cfg2, unsigned cfg3,
				unsigned cfg4, unsigned cfg5, unsigned cfg6, unsigned cfg7,
				unsigned cfg8, unsigned cfg9, unsigned cfg10, unsigned cfg11,
				unsigned cfg12, unsigned cfg13, unsigned cfg14, unsigned cfg15,
				unsigned cfg16, unsigned cfg17)
{
    /*--------------------------------------
     * Configuration register 0
     */
    printf("    DEVCFG0 = %08x\n", cfg0);
    if ((~cfg0 & MZ_CFG0_DEBUG_MASK) == MZ_CFG0_DEBUG_ENABLE)
        printf("                     %u Debugger enabled\n",
            MZ_CFG0_DEBUG_MASK);
    else
        printf("                     %u Debugger disabled\n",
            MZ_CFG0_DEBUG_MASK);

    if (~cfg0 & MZ_CFG0_JTAG_DISABLE)
        printf("                     %u JTAG disabled\n",
            MZ_CFG0_JTAG_DISABLE);
    else
        printf("                     %u JTAG enabled\n",
            MZ_CFG0_JTAG_DISABLE);

    if (~cfg0 & MZ_CFG0_ICESEL_PGE2)
        printf("                     %u Use PGC2/PGD2\n",
            MZ_CFG0_ICESEL_PGE2);

    if (~cfg0 & MZ_CFG0_TRC_DISABLE)
        printf("                    %u  Trace port disabled\n",
            MZ_CFG0_TRC_DISABLE >> 4);

    if (~cfg0 & MZ_CFG0_MICROMIPS)
        printf("                    %u  Boot in microMIPS mode\n",
            MZ_CFG0_MICROMIPS >> 4);

    switch (~cfg0 & MZ_CFG0_ECC_MASK) {
    case MZ_CFG0_ECC_ENABLE:
        printf("                   %u   Flash ECC enabled\n", MZ_CFG0_ECC_ENABLE >> 8);
        break;
    case MZ_CFG0_DECC_ENABLE:
        printf("                   %u   Dynamic Flash ECC enabled\n", MZ_CFG0_DECC_ENABLE >> 8);
        break;
    case MZ_CFG0_ECC_DIS_LOCK:
        printf("                   %u   Flash ECC disabled, locked\n", MZ_CFG0_ECC_DIS_LOCK >> 8);
        break;
    default:
        printf("                   %u   Flash ECC disabled, unlocked\n", MZ_CFG0_ECC_MASK >> 8);
        break;
    }

    if (~cfg0 & MZ_CFG0_FSLEEP)
        printf("                   %u   Flash power down controlled by VREGS bit\n",
            MZ_CFG0_FSLEEP >> 8);

    if (~cfg0 & MZ_CFG0_DBGPER0)
        printf("                  %u    Deny Debug access to group 0 regions\n",
            MZ_CFG0_DBGPER0 >> 12);
    if (~cfg0 & MZ_CFG0_DBGPER1)
        printf("                  %u    Deny Debug access to group 1 regions\n",
            MZ_CFG0_DBGPER1 >> 12);
    if (~cfg0 & MZ_CFG0_DBGPER2)
        printf("                  %u    Deny Debug access to group 2 regions\n",
            MZ_CFG0_DBGPER2 >> 12);

    if (~cfg0 & MZ_CFG0_EJTAG_REDUCED)
        printf("               %u       Reduced EJTAG functionality\n",
            MZ_CFG0_EJTAG_REDUCED >> 28);

    /*--------------------------------------
     * Configuration register 1
     */
    printf("    DEVCFG1 = %08x\n", cfg1);
    switch (cfg1 & MZ_CFG1_FNOSC_MASK) {
    case MZ_CFG1_FNOSC_SPLL:
        printf("                     %u System PLL\n", MZ_CFG1_FNOSC_SPLL);
        break;
    case MZ_CFG1_FNOSC_POSC:
        printf("                     %u Primary oscillator\n", MZ_CFG1_FNOSC_POSC);
        break;
    case MZ_CFG1_FNOSC_SOSC:
        printf("                     %u Secondary oscillator\n", MZ_CFG1_FNOSC_SOSC);
        break;
    case MZ_CFG1_FNOSC_LPRC:
        printf("                     %u Low-power RC oscillator\n", MZ_CFG1_FNOSC_LPRC);
        break;
    case MZ_CFG1_FNOSC_FRCDIV:
        printf("                     %u Fast RC oscillator with divide-by-N\n", MZ_CFG1_FNOSC_FRCDIV);
        break;
    default:
        printf("                     %u UNKNOWN\n", cfg1 & MZ_CFG1_FNOSC_MASK);
        break;
    }
    if (cfg1 & MZ_CFG1_FDMTEN) {
        switch (cfg1 & MZ_CFG1_DMTINV_MASK) {
        case MZ_CFG1_DMTINV_1_2:
            printf("                    %02x Deadman timer: 1/2\n", MZ_CFG1_DMTINV_1_2);
            break;
        case MZ_CFG1_DMTINV_3_4:
            printf("                    %02x Deadman timer: 3/4\n", MZ_CFG1_DMTINV_3_4);
            break;
        case MZ_CFG1_DMTINV_7_8:
            printf("                    %02x Deadman timer: 7/8\n", MZ_CFG1_DMTINV_7_8);
            break;
        case MZ_CFG1_DMTINV_15_16:
            printf("                    %02x Deadman timer: 15/16\n", MZ_CFG1_DMTINV_15_16);
            break;
        case MZ_CFG1_DMTINV_31_32:
            printf("                    %02x Deadman timer: 31/32\n", MZ_CFG1_DMTINV_31_32);
            break;
        case MZ_CFG1_DMTINV_63_64:
            printf("                    %02x Deadman timer: 63/64\n", MZ_CFG1_DMTINV_63_64);
            break;
        case MZ_CFG1_DMTINV_127_128:
            printf("                    %02x Deadman timer: 127/128\n", MZ_CFG1_DMTINV_127_128);
            break;
        }
    }
    if (cfg1 & MZ_CFG1_FSOSCEN)
        printf("                    %u  Secondary oscillator enabled\n",
            MZ_CFG1_FSOSCEN >> 4);
    if (cfg1 & MZ_CFG1_IESO)
        printf("                    %u  Internal-external switch over enabled\n",
            MZ_CFG1_IESO >> 4);

    switch (cfg1 & MZ_CFG1_POSCMOD_MASK) {
    case MZ_CFG1_POSCMOD_EXT:
        printf("                   %u   Primary oscillator: External\n", MZ_CFG1_POSCMOD_EXT >> 8);
        break;
    case MZ_CFG1_POSCMOD_HS:
        printf("                   %u   Primary oscillator: HS\n", MZ_CFG1_POSCMOD_HS >> 8);
        break;
    case MZ_CFG1_POSCMOD_DISABLE:
        printf("                   %u   Primary oscillator: disabled\n", MZ_CFG1_POSCMOD_DISABLE >> 8);
        break;
    }
    if (cfg1 & MZ_CFG1_CLKO_DISABLE)
        printf("                   %u   CLKO output disabled\n",
            MZ_CFG1_CLKO_DISABLE >> 8);
    if (cfg1 & MZ_CFG1_FCKS_ENABLE)
        printf("                  %u    Clock switching enabled\n",
            MZ_CFG1_FCKS_ENABLE >> 12);
    if (cfg1 & MZ_CFG1_FCKM_ENABLE)
        printf("                  %u    Fail-safe clock monitor enabled\n",
            MZ_CFG1_FCKM_ENABLE >> 12);

    if (cfg1 & MZ_CFG1_FWDTEN) {
        switch (cfg1 & MZ_CFG1_WDTPS_MASK) {
        case MZ_CFG1_WDTPS_1:
            printf("                %2x     Watchdog postscale: 1/1\n", MZ_CFG1_WDTPS_1 >> 16);
            break;
        case MZ_CFG1_WDTPS_2:
            printf("                %2x     Watchdog postscale: 1/2\n", MZ_CFG1_WDTPS_2 >> 16);
            break;
        case MZ_CFG1_WDTPS_4:
            printf("                %2x     Watchdog postscale: 1/4\n", MZ_CFG1_WDTPS_4 >> 16);
            break;
        case MZ_CFG1_WDTPS_8:
            printf("                %2x     Watchdog postscale: 1/8\n", MZ_CFG1_WDTPS_8 >> 16);
            break;
        case MZ_CFG1_WDTPS_16:
            printf("                %2x     Watchdog postscale: 1/16\n", MZ_CFG1_WDTPS_16 >> 16);
            break;
        case MZ_CFG1_WDTPS_32:
            printf("                %2x     Watchdog postscale: 1/32\n", MZ_CFG1_WDTPS_32 >> 16);
            break;
        case MZ_CFG1_WDTPS_64:
            printf("                %2x     Watchdog postscale: 1/64\n", MZ_CFG1_WDTPS_64 >> 16);
            break;
        case MZ_CFG1_WDTPS_128:
            printf("                %2x     Watchdog postscale: 1/128\n", MZ_CFG1_WDTPS_128 >> 16);
            break;
        case MZ_CFG1_WDTPS_256:
            printf("                %2x     Watchdog postscale: 1/256\n", MZ_CFG1_WDTPS_256 >> 16);
            break;
        case MZ_CFG1_WDTPS_512:
            printf("                %2x     Watchdog postscale: 1/512\n", MZ_CFG1_WDTPS_512 >> 16);
            break;
        case MZ_CFG1_WDTPS_1024:
            printf("                %2x     Watchdog postscale: 1/1024\n", MZ_CFG1_WDTPS_1024 >> 16);
            break;
        case MZ_CFG1_WDTPS_2048:
            printf("                %2x     Watchdog postscale: 1/2048\n", MZ_CFG1_WDTPS_2048 >> 16);
            break;
        case MZ_CFG1_WDTPS_4096:
            printf("                %2x     Watchdog postscale: 1/4096\n", MZ_CFG1_WDTPS_4096 >> 16);
            break;
        case MZ_CFG1_WDTPS_8192:
            printf("                %2x     Watchdog postscale: 1/8192\n", MZ_CFG1_WDTPS_8192 >> 16);
            break;
        case MZ_CFG1_WDTPS_16384:
            printf("                %2x     Watchdog postscale: 1/16384\n", MZ_CFG1_WDTPS_16384 >> 16);
            break;
        case MZ_CFG1_WDTPS_32768:
            printf("                %2x     Watchdog postscale: 1/32768\n", MZ_CFG1_WDTPS_32768 >> 16);
            break;
        case MZ_CFG1_WDTPS_65536:
            printf("                %2x     Watchdog postscale: 1/65536\n", MZ_CFG1_WDTPS_65536 >> 16);
            break;
        case MZ_CFG1_WDTPS_131072:
            printf("                %2x     Watchdog postscale: 1/131072\n", MZ_CFG1_WDTPS_131072 >> 16);
            break;
        case MZ_CFG1_WDTPS_262144:
            printf("                %2x     Watchdog postscale: 1/262144\n", MZ_CFG1_WDTPS_262144 >> 16);
            break;
        case MZ_CFG1_WDTPS_524288:
            printf("                %2x     Watchdog postscale: 1/524288\n", MZ_CFG1_WDTPS_524288 >> 16);
            break;
        case MZ_CFG1_WDTPS_1048576:
            printf("                %2x     Watchdog postscale: 1/1048576\n", MZ_CFG1_WDTPS_1048576 >> 16);
            break;
        }

        if (cfg1 & MZ_CFG1_WDTSPGM)
            printf("                %u      Watchdog stops during Flash programming\n",
                MZ_CFG1_WDTSPGM >> 20);
        if (cfg1 & MZ_CFG1_WINDIS)
            printf("                %u      Watchdog in non-Window mode\n",
                MZ_CFG1_WINDIS >> 20);

        switch (cfg1 & MZ_CFG1_FWDTWINSZ_MASK) {
        case MZ_CFG1_FWDTWINSZ_75:
            printf("               %x       Watchdog window size: 75%%\n", MZ_CFG1_FWDTWINSZ_75 >> 24);
            break;
        case MZ_CFG1_FWDTWINSZ_50:
            printf("               %x       Watchdog window size: 50%%\n", MZ_CFG1_FWDTWINSZ_50 >> 24);
            break;
        case MZ_CFG1_FWDTWINSZ_375:
            printf("               %x       Watchdog window size: 37.5%%\n", MZ_CFG1_FWDTWINSZ_375 >> 24);
            break;
        case MZ_CFG1_FWDTWINSZ_25:
            printf("               %x       Watchdog window size: 25%%\n", MZ_CFG1_FWDTWINSZ_25 >> 24);
            break;
        }

        printf("                %u      Watchdog enable\n",
            MZ_CFG1_FWDTEN >> 20);
    }
    if (cfg1 & MZ_CFG1_FDMTEN) {
        int dtc = (cfg1 >> 26) & 31;
        printf("             %02x        Deadman timer count: 2^%u\n", dtc, dtc + 8);

        printf("              %u        Deadman timer enable\n",
            MZ_CFG1_FDMTEN >> 28);
    }

    /*--------------------------------------
     * Configuration register 2
     */
    printf("    DEVCFG2 = %08x\n", cfg2);
    switch (cfg2 & MZ_CFG2_FPLLIDIV_MASK) {
    case MZ_CFG2_FPLLIDIV_1:
        printf("                     %u PLL divider: 1/1\n", MZ_CFG2_FPLLIDIV_1);
        break;
    case MZ_CFG2_FPLLIDIV_2:
        printf("                     %u PLL divider: 1/2\n", MZ_CFG2_FPLLIDIV_2);
        break;
    case MZ_CFG2_FPLLIDIV_3:
        printf("                     %u PLL divider: 1/3\n", MZ_CFG2_FPLLIDIV_3);
        break;
    case MZ_CFG2_FPLLIDIV_4:
        printf("                     %u PLL divider: 1/4\n", MZ_CFG2_FPLLIDIV_4);
        break;
    case MZ_CFG2_FPLLIDIV_5:
        printf("                     %u PLL divider: 1/5\n", MZ_CFG2_FPLLIDIV_5);
        break;
    case MZ_CFG2_FPLLIDIV_6:
        printf("                     %u PLL divider: 1/6\n", MZ_CFG2_FPLLIDIV_6);
        break;
    case MZ_CFG2_FPLLIDIV_7:
        printf("                     %u PLL divider: 1/7\n", MZ_CFG2_FPLLIDIV_7);
        break;
    case MZ_CFG2_FPLLIDIV_8:
        printf("                     %u PLL divider: 1/8\n", MZ_CFG2_FPLLIDIV_8);
        break;
    }
    switch (cfg2 & MZ_CFG2_FPLLRNG_MASK) {
    case MZ_CFG2_FPLLRNG_5_10:
        printf("                    %u  PLL input frequency range: 5-10 MHz\n", MZ_CFG2_FPLLRNG_5_10 >> 4);
        break;
    case MZ_CFG2_FPLLRNG_8_16:
        printf("                    %u  PLL input frequency range: 8-16 MHz\n", MZ_CFG2_FPLLRNG_8_16 >> 4);
        break;
    case MZ_CFG2_FPLLRNG_13_26:
        printf("                    %u  PLL input frequency range: 13-26 MHz\n", MZ_CFG2_FPLLRNG_13_26 >> 4);
        break;
    case MZ_CFG2_FPLLRNG_21_42:
        printf("                    %u  PLL input frequency range: 21-42 MHz\n", MZ_CFG2_FPLLRNG_21_42 >> 4);
        break;
    case MZ_CFG2_FPLLRNG_34_64:
        printf("                    %u  PLL input frequency range: 34-64 MHz\n", MZ_CFG2_FPLLRNG_34_64 >> 4);
        break;
    }
    if (cfg2 & MZ_CFG2_FPLLICLK_FRC)
        printf("                    %u  Select FRC as input to PLL\n",
            MZ_CFG2_FPLLICLK_FRC >> 4);

    int fpllmult = (cfg2 >> 8) & 0x3f;
    printf("                  %02x   PLL feedback divider: x%u\n", fpllmult, fpllmult + 1);

    switch (cfg2 & MZ_CFG2_FPLLODIV_MASK) {
    case MZ_CFG2_FPLLODIV_2:
    case MZ_CFG2_FPLLODIV_2a:
        printf("                 %u     PLL postscaler: 1/2\n", (cfg2 & MZ_CFG2_FPLLODIV_MASK) >> 16);
        break;
    case MZ_CFG2_FPLLODIV_4:
        printf("                 %u     PLL postscaler: 1/4\n", (cfg2 & MZ_CFG2_FPLLODIV_MASK) >> 16);
        break;
    case MZ_CFG2_FPLLODIV_8:
        printf("                 %u     PLL postscaler: 1/8\n", (cfg2 & MZ_CFG2_FPLLODIV_MASK) >> 16);
        break;
    case MZ_CFG2_FPLLODIV_16:
        printf("                 %u     PLL postscaler: 1/16\n", (cfg2 & MZ_CFG2_FPLLODIV_MASK) >> 16);
        break;
    case MZ_CFG2_FPLLODIV_32:
    case MZ_CFG2_FPLLODIV_32a:
    case MZ_CFG2_FPLLODIV_32b:
        printf("                 %u     PLL postscaler: 1/32\n", (cfg2 & MZ_CFG2_FPLLODIV_MASK) >> 16);
        break;
    }
    if (cfg2 & MZ_CFG2_UPLLEN) {
        if (cfg2 & MZ_CFG2_UPLLFSEL_24)
            printf("              %u        USB PLL input clock: 24 MHz\n",
                MZ_CFG2_UPLLFSEL_24 >> 28);
        printf("              %u        Enable USB PLL\n",
            MZ_CFG2_UPLLEN >> 28);
    }

    /*--------------------------------------
     * Configuration register 3
     */
    printf("    DEVCFG3 = %08x\n", cfg3);
    if (~cfg3 & MZ_CFG3_USERID_MASK)
        printf("                  %04x User-defined ID\n",
            cfg3 & MZ_CFG3_USERID_MASK);

    if (cfg3 & MZ_CFG3_FMIIEN)
        printf("               %u       Ethernet MII interface enable\n",
            MZ_CFG3_FMIIEN >> 24);

    if (cfg3 & MZ_CFG3_FETHIO)
        printf("               %u       Default Ethernet pins\n",
            MZ_CFG3_FETHIO >> 24);
    else
        printf("                       Alternate Ethernet pins\n");

    if (cfg3 & MZ_CFG3_PGL1WAY)
        printf("               %u       Permission Group Lock - only 1 reconfig\n",
            MZ_CFG3_PGL1WAY >> 24);

    if (cfg3 & MZ_CFG3_PMDL1WAY)
        printf("              %u        Peripheral Module Disable - only 1 reconfig\n",
            MZ_CFG3_PMDL1WAY >> 28);

    if (cfg3 & MZ_CFG3_IOL1WAY)
        printf("              %u        Peripheral Pin Select - only 1 reconfig\n",
            MZ_CFG3_IOL1WAY >> 28);

    if (cfg3 & MZ_CFG3_FUSBIDIO)
        printf("              %u        USBID pin: controlled by USB\n",
            MZ_CFG3_FUSBIDIO >> 28);
    else
        printf("                       USBID pin: controlled by port\n");
}
