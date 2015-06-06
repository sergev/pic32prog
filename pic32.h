/*
 * Microchip PIC32 jtag definitions.
 *
 * Copyright (C) 2011-2013 Serge Vakulenko
 *
 * This file is part of PIC32PROG project, which is distributed
 * under the terms of the GNU General Public License (GPL).
 * See the accompanying file "COPYING" for more details.
 */

#ifndef _PIC32_H
#define _PIC32_H
/*
 * Programing executive.
 * Described in PIC32MX Flash Programming Specification.
 */
#define	PIC32_PE_LOADER_LEN 42

extern const unsigned short pic32_pe_loader[];
extern const unsigned pic32_pemx1[];
extern const unsigned pic32_pemx3[];
extern const unsigned pic32_pemz[];

/*
 * TAP instructions (5-bit).
 */
#define TAP_SW_MTAP     4       // Switch to MCHP TAP controller
#define TAP_SW_ETAP     5       // Switch to EJTAG TAP controller

/*
 * MTAP-specific instructions.
 */
#define MTAP_IDCODE     1       // Select chip identification register
#define MTAP_COMMAND    7       // Connect to MCHP command register

/*
 * ETAP-specific instructions.
 */
#define ETAP_IDCODE     1       // Device identification
#define ETAP_IMPCODE    3       // Implementation register
#define ETAP_ADDRESS    8       // Select Address register
#define ETAP_DATA       9       // Select Data register
#define ETAP_CONTROL    10      // Select EJTAG Control register
#define ETAP_ALL        11      // Select Address, Data and Control registers
#define ETAP_EJTAGBOOT  12      // On reset, take debug exception
#define ETAP_NORMALBOOT 13      // On reset, enter reset handler
#define ETAP_FASTDATA   14      // Select FastData register

/*
 * Microchip DR commands (32-bit).
 */
#define MCHP_STATUS        0x00 // Return Status
#define MCHP_ASSERT_RST    0xD1 // Assert device reset
#define MCHP_DEASSERT_RST  0xD0 // Remove device reset
#define MCHP_ERASE         0xFC // Flash chip erase
#define MCHP_FLASH_ENABLE  0xFE // Enable access from CPU to flash
#define MCHP_FLASH_DISABLE 0xFD // Disable access from CPU to flash

/*
 * MCHP status value.
 */
#define MCHP_STATUS_CPS    0x80 // Device is NOT code-protected
#define MCHP_STATUS_NVMERR 0x20 // Error occured during NVM operation
#define MCHP_STATUS_CFGRDY 0x08 // Configuration has been read and
                                // Code-Protect State bit is valid
#define MCHP_STATUS_FCBUSY 0x04 // Flash Controller is Busy (erase is in progress)
#define MCHP_STATUS_FAEN   0x02 // Flash access is enabled
#define MCHP_STATUS_DEVRST 0x01 // Device reset is active

/*
 * EJTAG Control register.
 */
#define CONTROL_ROCC            (1 << 31)   /* Reset occured */
#define CONTROL_PSZ_MASK        (3 << 29)   /* Size of pending access */
#define CONTROL_PSZ_BYTE        (0 << 29)   /* Byte */
#define CONTROL_PSZ_HALFWORD    (1 << 29)   /* Half-word */
#define CONTROL_PSZ_WORD        (2 << 29)   /* Word */
#define CONTROL_PSZ_TRIPLE      (3 << 29)   /* Triple, double-word */
#define CONTROL_VPED            (1 << 23)   /* VPE disabled */
#define CONTROL_DOZE            (1 << 22)   /* Processor in low-power mode */
#define CONTROL_HALT            (1 << 21)   /* System bus clock stopped */
#define CONTROL_PERRST          (1 << 20)   /* Peripheral reset applied */
#define CONTROL_PRNW            (1 << 19)   /* Store access */
#define CONTROL_PRACC           (1 << 18)   /* Pending processor access */
#define CONTROL_RDVEC           (1 << 17)   /* Relocatable debug exception vector */
#define CONTROL_PRRST           (1 << 16)   /* Processor reset applied */
#define CONTROL_PROBEN          (1 << 15)   /* Probe will service processor accesses */
#define CONTROL_PROBTRAP        (1 << 14)   /* Debug vector at ff200200 */
#define CONTROL_EJTAGBRK        (1 << 12)   /* Debug interrupt exception */
#define CONTROL_DM              (1 << 3)    /* Debug mode */

/*
 * PE command set.
 */
#define PE_ROW_PROGRAM          0x0     /* Program one row of flash memory */
#define PE_READ                 0x1     /* Read N 32-bit words */
#define PE_PROGRAM              0x2     /* Program flash memory */
#define PE_WORD_PROGRAM         0x3     /* Program one word of flash memory */
#define PE_CHIP_ERASE           0x4     /* Erase the entire chip */
#define PE_PAGE_ERASE           0x5     /* Erase pages by address */
#define PE_BLANK_CHECK          0x6     /* Check blank memory */
#define PE_EXEC_VERSION         0x7     /* Read the PE software version */
#define PE_GET_CRC              0x8     /* Get the checksum of memory */
#define PE_PROGRAM_CLUSTER      0x9     /* Program N bytes */
#define PE_GET_DEVICEID         0xA     /* Return the hardware ID of device */
#define PE_CHANGE_CFG           0xB     /* Change PE settings */

/*-------------------------------------------------------------------
 * MX3/4/5/6/7 family.
 *
 * Config0 register, inverted.
 */
#define MX3_CFG0_DEBUG_MASK     0x00000003 /* Debugger enable bits */
#define MX3_CFG0_DEBUG_DISABLED 0x00000000 /* Debugger disabled */
#define MX3_CFG0_DEBUG_ENABLED  0x00000002 /* Debugger enabled */
#define MX3_CFG0_JTAG_DISABLE   0x00000004 /* Disable JTAG port */
#define MX3_CFG0_ICESEL_MASK    0x00000018 /* Debugger channel select */
#define MX3_CFG0_ICESEL_PAIR1   0x00000000 /* Use PGC1/PGD1 */
#define MX3_CFG0_ICESEL_PAIR2   0x00000008 /* Use PGC2/PGD2 */
#define MX3_CFG0_ICESEL_PAIR3   0x00000010 /* Use PGC3/PGD3 */
#define MX3_CFG0_ICESEL_PAIR4   0x00000018 /* Use PGC4/PGD4 */
#define MX3_CFG0_PWP_MASK       0x000ff000 /* Program flash write protect */
#define MX3_CFG0_BWP            0x01000000 /* Boot flash write protect */
#define MX3_CFG0_CP             0x10000000 /* Code protect */

/*
 * Config1 register.
 */
#define MX3_CFG1_FNOSC_MASK     0x00000007 /* Oscillator selection */
#define MX3_CFG1_FNOSC_FRC      0x00000000 /* Fast RC */
#define MX3_CFG1_FNOSC_FRCDIVPLL 0x00000001 /* Fast RC with divide-by-N and PLL */
#define MX3_CFG1_FNOSC_PRI      0x00000002 /* Primary oscillator XT, HS, EC */
#define MX3_CFG1_FNOSC_PRIPLL   0x00000003 /* Primary with PLL */
#define MX3_CFG1_FNOSC_SEC      0x00000004 /* Secondary oscillator */
#define MX3_CFG1_FNOSC_LPRC     0x00000005 /* Low-power RC */
#define MX3_CFG1_FNOSC_FRCDIV16 0x00000006 /* Fast RC with divide-by-16 */
#define MX3_CFG1_FNOSC_FRCDIV   0x00000007 /* Fast RC with divide-by-N */
#define MX3_CFG1_FSOSCEN        0x00000020 /* Secondary oscillator enable */
#define MX3_CFG1_IESO           0x00000080 /* Internal-external switch over */
#define MX3_CFG1_POSCMOD_MASK   0x00000300 /* Primary oscillator config */
#define MX3_CFG1_POSCMOD_EXT    0x00000000 /* External mode */
#define MX3_CFG1_POSCMOD_XT     0x00000100 /* XT oscillator */
#define MX3_CFG1_POSCMOD_HS     0x00000200 /* HS oscillator */
#define MX3_CFG1_POSCMOD_DISABLE 0x00000300 /* Disabled */
#define MX3_CFG1_CLKO_DISABLE   0x00000400 /* Disable CLKO output */
#define MX3_CFG1_FPBDIV_MASK    0x00003000 /* Peripheral bus clock divisor */
#define MX3_CFG1_FPBDIV_1       0x00000000 /* SYSCLK / 1 */
#define MX3_CFG1_FPBDIV_2       0x00001000 /* SYSCLK / 2 */
#define MX3_CFG1_FPBDIV_4       0x00002000 /* SYSCLK / 4 */
#define MX3_CFG1_FPBDIV_8       0x00003000 /* SYSCLK / 8 */
#define MX3_CFG1_FCKM_DISABLE   0x00004000 /* Fail-safe clock monitor disable */
#define MX3_CFG1_FCKS_DISABLE   0x00008000 /* Clock switching disable */
#define MX3_CFG1_WDTPS_MASK     0x001f0000 /* Watchdog postscale */
#define MX3_CFG1_WDTPS_1        0x00000000 /* 1:1 */
#define MX3_CFG1_WDTPS_2        0x00010000 /* 1:2 */
#define MX3_CFG1_WDTPS_4        0x00020000 /* 1:4 */
#define MX3_CFG1_WDTPS_8        0x00030000 /* 1:8 */
#define MX3_CFG1_WDTPS_16       0x00040000 /* 1:16 */
#define MX3_CFG1_WDTPS_32       0x00050000 /* 1:32 */
#define MX3_CFG1_WDTPS_64       0x00060000 /* 1:64 */
#define MX3_CFG1_WDTPS_128      0x00070000 /* 1:128 */
#define MX3_CFG1_WDTPS_256      0x00080000 /* 1:256 */
#define MX3_CFG1_WDTPS_512      0x00090000 /* 1:512 */
#define MX3_CFG1_WDTPS_1024     0x000a0000 /* 1:1024 */
#define MX3_CFG1_WDTPS_2048     0x000b0000 /* 1:2048 */
#define MX3_CFG1_WDTPS_4096     0x000c0000 /* 1:4096 */
#define MX3_CFG1_WDTPS_8192     0x000d0000 /* 1:8192 */
#define MX3_CFG1_WDTPS_16384    0x000e0000 /* 1:16384 */
#define MX3_CFG1_WDTPS_32768    0x000f0000 /* 1:32768 */
#define MX3_CFG1_WDTPS_65536    0x00100000 /* 1:65536 */
#define MX3_CFG1_WDTPS_131072   0x00110000 /* 1:131072 */
#define MX3_CFG1_WDTPS_262144   0x00120000 /* 1:262144 */
#define MX3_CFG1_WDTPS_524288   0x00130000 /* 1:524288 */
#define MX3_CFG1_WDTPS_1048576  0x00140000 /* 1:1048576 */
#define MX3_CFG1_FWDTEN         0x00800000 /* Watchdog enable */

/*
 * Config2 register.
 */
#define MX3_CFG2_FPLLIDIV_MASK  0x00000007 /* PLL input divider */
#define MX3_CFG2_FPLLIDIV_1     0x00000000 /* 1x */
#define MX3_CFG2_FPLLIDIV_2     0x00000001 /* 2x */
#define MX3_CFG2_FPLLIDIV_3     0x00000002 /* 3x */
#define MX3_CFG2_FPLLIDIV_4     0x00000003 /* 4x */
#define MX3_CFG2_FPLLIDIV_5     0x00000004 /* 5x */
#define MX3_CFG2_FPLLIDIV_6     0x00000005 /* 6x */
#define MX3_CFG2_FPLLIDIV_10    0x00000006 /* 10x */
#define MX3_CFG2_FPLLIDIV_12    0x00000007 /* 12x */
#define MX3_CFG2_FPLLMUL_MASK   0x00000070 /* PLL multiplier */
#define MX3_CFG2_FPLLMUL_15     0x00000000 /* 15x */
#define MX3_CFG2_FPLLMUL_16     0x00000010 /* 16x */
#define MX3_CFG2_FPLLMUL_17     0x00000020 /* 17x */
#define MX3_CFG2_FPLLMUL_18     0x00000030 /* 18x */
#define MX3_CFG2_FPLLMUL_19     0x00000040 /* 19x */
#define MX3_CFG2_FPLLMUL_20     0x00000050 /* 20x */
#define MX3_CFG2_FPLLMUL_21     0x00000060 /* 21x */
#define MX3_CFG2_FPLLMUL_24     0x00000070 /* 24x */
#define MX3_CFG2_UPLLIDIV_MASK  0x00000700 /* USB PLL input divider */
#define MX3_CFG2_UPLLIDIV_1     0x00000000 /* 1x */
#define MX3_CFG2_UPLLIDIV_2     0x00000100 /* 2x */
#define MX3_CFG2_UPLLIDIV_3     0x00000200 /* 3x */
#define MX3_CFG2_UPLLIDIV_4     0x00000300 /* 4x */
#define MX3_CFG2_UPLLIDIV_5     0x00000400 /* 5x */
#define MX3_CFG2_UPLLIDIV_6     0x00000500 /* 6x */
#define MX3_CFG2_UPLLIDIV_10    0x00000600 /* 10x */
#define MX3_CFG2_UPLLIDIV_12    0x00000700 /* 12x */
#define MX3_CFG2_UPLL_DISABLE   0x00008000 /* Disable USB PLL */
#define MX3_CFG2_FPLLODIV_MASK  0x00070000 /* Default postscaler for PLL */
#define MX3_CFG2_FPLLODIV_1     0x00000000 /* 1x */
#define MX3_CFG2_FPLLODIV_2     0x00010000 /* 2x */
#define MX3_CFG2_FPLLODIV_4     0x00020000 /* 4x */
#define MX3_CFG2_FPLLODIV_8     0x00030000 /* 8x */
#define MX3_CFG2_FPLLODIV_16    0x00040000 /* 16x */
#define MX3_CFG2_FPLLODIV_32    0x00050000 /* 32x */
#define MX3_CFG2_FPLLODIV_64    0x00060000 /* 64x */
#define MX3_CFG2_FPLLODIV_256   0x00070000 /* 256x */

/*
 * Config3 register.
 */
#define MX3_CFG3_USERID_MASK    0x0000ffff /* User-defined ID */
#define MX3_CFG3_FSRSSEL_MASK   0x00070000 /* SRS select */
#define MX3_CFG3_FSRSSEL_ALL    0x00000000 /* All irqs assigned to shadow set */
#define MX3_CFG3_FSRSSEL_1      0x00010000 /* Assign irq priority 1 to shadow set */
#define MX3_CFG3_FSRSSEL_2      0x00020000 /* Assign irq priority 2 to shadow set */
#define MX3_CFG3_FSRSSEL_3      0x00030000 /* Assign irq priority 3 to shadow set */
#define MX3_CFG3_FSRSSEL_4      0x00040000 /* Assign irq priority 4 to shadow set */
#define MX3_CFG3_FSRSSEL_5      0x00050000 /* Assign irq priority 5 to shadow set */
#define MX3_CFG3_FSRSSEL_6      0x00060000 /* Assign irq priority 6 to shadow set */
#define MX3_CFG3_FSRSSEL_7      0x00070000 /* Assign irq priority 7 to shadow set */
#define MX3_CFG3_FMIIEN         0x01000000 /* Ethernet MII enable */
#define MX3_CFG3_FETHIO         0x02000000 /* Ethernet pins default */
#define MX3_CFG3_FCANIO         0x04000000 /* CAN pins default */
#define MX3_CFG3_FUSBIDIO       0x40000000 /* USBID pin: controlled by USB */
#define MX3_CFG3_FVBUSONIO      0x80000000 /* VBuson pin: controlled by USB */

/*-------------------------------------------------------------------
 * MZ family.
 *
 * Config0 register, inverted.
 */
#define MZ_CFG0_DEBUG_MASK      0x00000003 /* Background debugger mode */
#define MZ_CFG0_DEBUG_DISABLE   0x00000000 /* Disable debugger */
#define MZ_CFG0_DEBUG_ENABLE    0x00000002 /* Enable background debugger */
#define MZ_CFG0_JTAG_DISABLE    0x00000004 /* Disable JTAG port */
#define MZ_CFG0_ICESEL_PGE2     0x00000008 /* Use PGC2/PGD2 (default PGC1/PGD1) */
#define MZ_CFG0_TRC_DISABLE     0x00000020 /* Disable Trace port */
#define MZ_CFG0_MICROMIPS       0x00000040 /* Boot in microMIPS mode */
#define MZ_CFG0_ECC_MASK        0x00000300 /* Enable Flash ECC */
#define MZ_CFG0_ECC_ENABLE      0x00000300 /* Enable Flash ECC */
#define MZ_CFG0_DECC_ENABLE     0x00000200 /* Enable Dynamic Flash ECC */
#define MZ_CFG0_ECC_DIS_LOCK    0x00000100 /* Disable ECC, lock ECCCON */
#define MZ_CFG0_FSLEEP          0x00000400 /* Flash power down controlled by VREGS bit */
#define MZ_CFG0_DBGPER0         0x00001000 /* In Debug mode, deny CPU access to
                                            * Permission Group 0 permission regions */
#define MZ_CFG0_DBGPER1         0x00002000 /* In Debug mode, deny CPU access to
                                            * Permission Group 1 permission regions */
#define MZ_CFG0_DBGPER2         0x00004000 /* In Debug mode, deny CPU access to
                                            * Permission Group 1 permission regions */
#define MZ_CFG0_EJTAG_REDUCED   0x40000000 /* Reduced EJTAG functionality */

/*
 * Config1 register.
 */
#define MZ_CFG1_UNUSED          0x00003800
#define MZ_CFG1_FNOSC_MASK      0x00000007 /* Oscillator selection */
#define MZ_CFG1_FNOSC_SPLL      0x00000001 /* SPLL */
#define MZ_CFG1_FNOSC_POSC      0x00000002 /* Primary oscillator XT, HS, EC */
#define MZ_CFG1_FNOSC_SOSC      0x00000004 /* Secondary oscillator */
#define MZ_CFG1_FNOSC_LPRC      0x00000005 /* Low-power RC */
#define MZ_CFG1_FNOSC_FRCDIV    0x00000007 /* Fast RC with divide-by-N */
#define MZ_CFG1_DMTINV_MASK     0x00000038 /* Deadman Timer Count Window Interval */
#define MZ_CFG1_DMTINV_1_2      0x00000008 /* 1/2 of counter value */
#define MZ_CFG1_DMTINV_3_4      0x00000010 /* 3/4 of counter value */
#define MZ_CFG1_DMTINV_7_8      0x00000018 /* 7/8 of counter value */
#define MZ_CFG1_DMTINV_15_16    0x00000020 /* 15/16 of counter value */
#define MZ_CFG1_DMTINV_31_32    0x00000028 /* 31/32 of counter value */
#define MZ_CFG1_DMTINV_63_64    0x00000030 /* 63/64 of counter value */
#define MZ_CFG1_DMTINV_127_128  0x00000038 /* 127/128 of counter value */
#define MZ_CFG1_FSOSCEN         0x00000040 /* Secondary oscillator enable */
#define MZ_CFG1_IESO            0x00000080 /* Internal-external switch over */
#define MZ_CFG1_POSCMOD_MASK    0x00000300 /* Primary oscillator config */
#define MZ_CFG1_POSCMOD_EXT     0x00000000 /* External mode */
#define MZ_CFG1_POSCMOD_HS      0x00000200 /* HS oscillator */
#define MZ_CFG1_POSCMOD_DISABLE 0x00000300 /* Disabled */
#define MZ_CFG1_CLKO_DISABLE    0x00000400 /* Disable CLKO output */
#define MZ_CFG1_FCKS_ENABLE     0x00004000 /* Enable clock switching */
#define MZ_CFG1_FCKM_ENABLE     0x00008000 /* Enable fail-safe clock monitoring */
#define MZ_CFG1_WDTPS_MASK      0x001f0000 /* Watchdog postscale */
#define MZ_CFG1_WDTPS_1         0x00000000 /* 1:1 */
#define MZ_CFG1_WDTPS_2         0x00010000 /* 1:2 */
#define MZ_CFG1_WDTPS_4         0x00020000 /* 1:4 */
#define MZ_CFG1_WDTPS_8         0x00030000 /* 1:8 */
#define MZ_CFG1_WDTPS_16        0x00040000 /* 1:16 */
#define MZ_CFG1_WDTPS_32        0x00050000 /* 1:32 */
#define MZ_CFG1_WDTPS_64        0x00060000 /* 1:64 */
#define MZ_CFG1_WDTPS_128       0x00070000 /* 1:128 */
#define MZ_CFG1_WDTPS_256       0x00080000 /* 1:256 */
#define MZ_CFG1_WDTPS_512       0x00090000 /* 1:512 */
#define MZ_CFG1_WDTPS_1024      0x000a0000 /* 1:1024 */
#define MZ_CFG1_WDTPS_2048      0x000b0000 /* 1:2048 */
#define MZ_CFG1_WDTPS_4096      0x000c0000 /* 1:4096 */
#define MZ_CFG1_WDTPS_8192      0x000d0000 /* 1:8192 */
#define MZ_CFG1_WDTPS_16384     0x000e0000 /* 1:16384 */
#define MZ_CFG1_WDTPS_32768     0x000f0000 /* 1:32768 */
#define MZ_CFG1_WDTPS_65536     0x00100000 /* 1:65536 */
#define MZ_CFG1_WDTPS_131072    0x00110000 /* 1:131072 */
#define MZ_CFG1_WDTPS_262144    0x00120000 /* 1:262144 */
#define MZ_CFG1_WDTPS_524288    0x00130000 /* 1:524288 */
#define MZ_CFG1_WDTPS_1048576   0x00140000 /* 1:1048576 */
#define MZ_CFG1_WDTSPGM         0x00200000 /* Watchdog stops during Flash programming */
#define MZ_CFG1_WINDIS          0x00400000 /* Watchdog is in non-Window mode */
#define MZ_CFG1_FWDTEN          0x00800000 /* Watchdog enable */
#define MZ_CFG1_FWDTWINSZ_MASK  0x03000000 /* Watchdog window size */
#define MZ_CFG1_FWDTWINSZ_75    0x00000000 /* 75% */
#define MZ_CFG1_FWDTWINSZ_50    0x01000000 /* 50% */
#define MZ_CFG1_FWDTWINSZ_375   0x02000000 /* 37.5% */
#define MZ_CFG1_FWDTWINSZ_25    0x03000000 /* 25% */
#define MZ_CFG1_DMTCNT(n)       ((n)<<26)  /* Deadman Timer Count Select */
#define MZ_CFG1_FDMTEN          0x80000000 /* Deadman Timer enable */

/*
 * Config2 register.
 */
#define MZ_CFG2_UNUSED          0x3ff88008
#define MZ_CFG2_FPLLIDIV_MASK   0x00000007 /* PLL input divider */
#define MZ_CFG2_FPLLIDIV_1      0x00000000 /* 1x */
#define MZ_CFG2_FPLLIDIV_2      0x00000001 /* 2x */
#define MZ_CFG2_FPLLIDIV_3      0x00000002 /* 3x */
#define MZ_CFG2_FPLLIDIV_4      0x00000003 /* 4x */
#define MZ_CFG2_FPLLIDIV_5      0x00000004 /* 5x */
#define MZ_CFG2_FPLLIDIV_6      0x00000005 /* 6x */
#define MZ_CFG2_FPLLIDIV_7      0x00000006 /* 7x */
#define MZ_CFG2_FPLLIDIV_8      0x00000007 /* 8x */
#define MZ_CFG2_FPLLRNG_MASK    0x00000070 /* PLL input frequency range */
#define MZ_CFG2_FPLLRNG_5_10    0x00000010 /* 5-10 MHz */
#define MZ_CFG2_FPLLRNG_8_16    0x00000020 /* 8-16 MHz */
#define MZ_CFG2_FPLLRNG_13_26   0x00000030 /* 13-26 MHz */
#define MZ_CFG2_FPLLRNG_21_42   0x00000040 /* 21-42 MHz */
#define MZ_CFG2_FPLLRNG_34_64   0x00000050 /* 34-64 MHz */
#define MZ_CFG2_FPLLICLK_FRC    0x00000080 /* Select FRC as input to PLL */
#define MZ_CFG1_FPLLMULT(n)     (((n)-1)<<8) /* PLL Feedback Divider */
#define MZ_CFG2_FPLLODIV_MASK   0x00070000 /* Default PLL output divisor */
#define MZ_CFG2_FPLLODIV_2      0x00000000 /* 2x */
#define MZ_CFG2_FPLLODIV_2a     0x00010000 /* 2x */
#define MZ_CFG2_FPLLODIV_4      0x00020000 /* 4x */
#define MZ_CFG2_FPLLODIV_8      0x00030000 /* 8x */
#define MZ_CFG2_FPLLODIV_16     0x00040000 /* 16x */
#define MZ_CFG2_FPLLODIV_32     0x00050000 /* 32x */
#define MZ_CFG2_FPLLODIV_32a    0x00060000 /* 32x */
#define MZ_CFG2_FPLLODIV_32b    0x00070000 /* 32x */
#define MZ_CFG2_UPLLFSEL_24     0x40000000 /* USB PLL input clock is 24 MHz (default 12 MHz) */
#define MZ_CFG2_UPLLEN          0x80000000 /* Enable USB PLL */

/*
 * Config3 register.
 */
#define MZ_CFG3_UNUSED          0x84ff0000
#define MZ_CFG3_USERID_MASK     0x0000ffff /* User-defined ID */
#define MZ_CFG3_USERID(x)       ((x) & 0xffff)
#define MZ_CFG3_FMIIEN          0x01000000 /* Ethernet MII enable */
#define MZ_CFG3_FETHIO          0x02000000 /* Default Ethernet pins */
#define MZ_CFG3_PGL1WAY         0x08000000 /* Permission Group Lock - only 1 reconfig */
#define MZ_CFG3_PMDL1WAY        0x10000000 /* Peripheral Module Disable - only 1 reconfig */
#define MZ_CFG3_IOL1WAY         0x20000000 /* Peripheral Pin Select - only 1 reconfig */
#define MZ_CFG3_FUSBIDIO        0x40000000 /* USBID pin: controlled by USB */

/*-------------------------------------------------------------------
 * MX1/2 family.
 *
 * Config0 register, inverted.
 */
#define MX1_CFG0_DEBUG_MASK     0x00000003 /* Debugger enable bits */
#define MX1_CFG0_DEBUG_DISABLED 0x00000000 /* Debugger disabled */
#define MX1_CFG0_DEBUG_ENABLED  0x00000002 /* Debugger enabled */
#define MX1_CFG0_JTAG_DISABLE   0x00000004 /* Disable JTAG port */
#define MX1_CFG0_ICESEL_MASK    0x00000018 /* Debugger channel select */
#define MX1_CFG0_ICESEL_PAIR1   0x00000000 /* Use PGC1/PGD1 */
#define MX1_CFG0_ICESEL_PAIR2   0x00000008 /* Use PGC2/PGD2 */
#define MX1_CFG0_ICESEL_PAIR3   0x00000010 /* Use PGC3/PGD3 */
#define MX1_CFG0_ICESEL_PAIR4   0x00000018 /* Use PGC4/PGD4 */
#define MX1_CFG0_PWP_MASK       0x0000fc00 /* Program flash write protect */
#define MX1_CFG0_BWP            0x01000000 /* Boot flash write protect */
#define MX1_CFG0_CP             0x10000000 /* Code protect */

/*
 * Config1 register.
 */
#define MX1_CFG1_FNOSC_MASK     0x00000007 /* Oscillator selection */
#define MX1_CFG1_FNOSC_FRC      0x00000000 /* Fast RC */
#define MX1_CFG1_FNOSC_FRCDIVPLL 0x00000001 /* Fast RC with divide-by-N and PLL */
#define MX1_CFG1_FNOSC_PRI      0x00000002 /* Primary oscillator XT, HS, EC */
#define MX1_CFG1_FNOSC_PRIPLL   0x00000003 /* Primary with PLL */
#define MX1_CFG1_FNOSC_SEC      0x00000004 /* Secondary oscillator */
#define MX1_CFG1_FNOSC_LPRC     0x00000005 /* Low-power RC */
#define MX1_CFG1_FNOSC_FRCDIV16 0x00000006 /* Fast RC with divide-by-16 */
#define MX1_CFG1_FNOSC_FRCDIV   0x00000007 /* Fast RC with divide-by-N */
#define MX1_CFG1_FSOSCEN        0x00000020 /* Secondary oscillator enable */
#define MX1_CFG1_IESO           0x00000080 /* Internal-external switch over */
#define MX1_CFG1_POSCMOD_MASK   0x00000300 /* Primary oscillator config */
#define MX1_CFG1_POSCMOD_EXT    0x00000000 /* External mode */
#define MX1_CFG1_POSCMOD_XT     0x00000100 /* XT oscillator */
#define MX1_CFG1_POSCMOD_HS     0x00000200 /* HS oscillator */
#define MX1_CFG1_POSCMOD_DISABLE 0x00000300 /* Disabled */
#define MX1_CFG1_CLKO_DISABLE   0x00000400 /* Disable CLKO output */
#define MX1_CFG1_FPBDIV_MASK    0x00003000 /* Peripheral bus clock divisor */
#define MX1_CFG1_FPBDIV_1       0x00000000 /* SYSCLK / 1 */
#define MX1_CFG1_FPBDIV_2       0x00001000 /* SYSCLK / 2 */
#define MX1_CFG1_FPBDIV_4       0x00002000 /* SYSCLK / 4 */
#define MX1_CFG1_FPBDIV_8       0x00003000 /* SYSCLK / 8 */
#define MX1_CFG1_FCKS_ENABLE    0x00004000 /* Clock switching enable */
#define MX1_CFG1_FCKM_ENABLE    0x00008000 /* Fail-safe clock monitor enable */
#define MX1_CFG1_WDTPS_MASK     0x001f0000 /* Watchdog postscale */
#define MX1_CFG1_WDTPS_1        0x00000000 /* 1:1 */
#define MX1_CFG1_WDTPS_2        0x00010000 /* 1:2 */
#define MX1_CFG1_WDTPS_4        0x00020000 /* 1:4 */
#define MX1_CFG1_WDTPS_8        0x00030000 /* 1:8 */
#define MX1_CFG1_WDTPS_16       0x00040000 /* 1:16 */
#define MX1_CFG1_WDTPS_32       0x00050000 /* 1:32 */
#define MX1_CFG1_WDTPS_64       0x00060000 /* 1:64 */
#define MX1_CFG1_WDTPS_128      0x00070000 /* 1:128 */
#define MX1_CFG1_WDTPS_256      0x00080000 /* 1:256 */
#define MX1_CFG1_WDTPS_512      0x00090000 /* 1:512 */
#define MX1_CFG1_WDTPS_1024     0x000a0000 /* 1:1024 */
#define MX1_CFG1_WDTPS_2048     0x000b0000 /* 1:2048 */
#define MX1_CFG1_WDTPS_4096     0x000c0000 /* 1:4096 */
#define MX1_CFG1_WDTPS_8192     0x000d0000 /* 1:8192 */
#define MX1_CFG1_WDTPS_16384    0x000e0000 /* 1:16384 */
#define MX1_CFG1_WDTPS_32768    0x000f0000 /* 1:32768 */
#define MX1_CFG1_WDTPS_65536    0x00100000 /* 1:65536 */
#define MX1_CFG1_WDTPS_131072   0x00110000 /* 1:131072 */
#define MX1_CFG1_WDTPS_262144   0x00120000 /* 1:262144 */
#define MX1_CFG1_WDTPS_524288   0x00130000 /* 1:524288 */
#define MX1_CFG1_WDTPS_1048576  0x00140000 /* 1:1048576 */
#define MX1_CFG1_WINDIS         0x00400000 /* Watchdog is in non-Window mode */
#define MX1_CFG1_FWDTEN         0x00800000 /* Watchdog enable */

/*
 * Config2 register.
 */
#define MX1_CFG2_FPLLIDIV_MASK  0x00000007 /* PLL input divider */
#define MX1_CFG2_FPLLIDIV_1     0x00000000 /* 1x */
#define MX1_CFG2_FPLLIDIV_2     0x00000001 /* 2x */
#define MX1_CFG2_FPLLIDIV_3     0x00000002 /* 3x */
#define MX1_CFG2_FPLLIDIV_4     0x00000003 /* 4x */
#define MX1_CFG2_FPLLIDIV_5     0x00000004 /* 5x */
#define MX1_CFG2_FPLLIDIV_6     0x00000005 /* 6x */
#define MX1_CFG2_FPLLIDIV_10    0x00000006 /* 10x */
#define MX1_CFG2_FPLLIDIV_12    0x00000007 /* 12x */
#define MX1_CFG2_FPLLMUL_MASK   0x00000070 /* PLL multiplier */
#define MX1_CFG2_FPLLMUL_15     0x00000000 /* 15x */
#define MX1_CFG2_FPLLMUL_16     0x00000010 /* 16x */
#define MX1_CFG2_FPLLMUL_17     0x00000020 /* 17x */
#define MX1_CFG2_FPLLMUL_18     0x00000030 /* 18x */
#define MX1_CFG2_FPLLMUL_19     0x00000040 /* 19x */
#define MX1_CFG2_FPLLMUL_20     0x00000050 /* 20x */
#define MX1_CFG2_FPLLMUL_21     0x00000060 /* 21x */
#define MX1_CFG2_FPLLMUL_24     0x00000070 /* 24x */
#define MX1_CFG2_UPLLIDIV_MASK  0x00000700 /* USB PLL input divider */
#define MX1_CFG2_UPLLIDIV_1     0x00000000 /* 1x */
#define MX1_CFG2_UPLLIDIV_2     0x00000100 /* 2x */
#define MX1_CFG2_UPLLIDIV_3     0x00000200 /* 3x */
#define MX1_CFG2_UPLLIDIV_4     0x00000300 /* 4x */
#define MX1_CFG2_UPLLIDIV_5     0x00000400 /* 5x */
#define MX1_CFG2_UPLLIDIV_6     0x00000500 /* 6x */
#define MX1_CFG2_UPLLIDIV_10    0x00000600 /* 10x */
#define MX1_CFG2_UPLLIDIV_12    0x00000700 /* 12x */
#define MX1_CFG2_UPLL_DISABLE   0x00008000 /* Disable and bypass USB PLL */
#define MX1_CFG2_FPLLODIV_MASK  0x00070000 /* Default postscaler for PLL */
#define MX1_CFG2_FPLLODIV_1     0x00000000 /* 1x */
#define MX1_CFG2_FPLLODIV_2     0x00010000 /* 2x */
#define MX1_CFG2_FPLLODIV_4     0x00020000 /* 4x */
#define MX1_CFG2_FPLLODIV_8     0x00030000 /* 8x */
#define MX1_CFG2_FPLLODIV_16    0x00040000 /* 16x */
#define MX1_CFG2_FPLLODIV_32    0x00050000 /* 32x */
#define MX1_CFG2_FPLLODIV_64    0x00060000 /* 64x */
#define MX1_CFG2_FPLLODIV_256   0x00070000 /* 256x */

/*
 * Config3 register.
 */
#define MX1_CFG3_USERID_MASK    0x0000ffff /* User-defined ID */
#define MX1_CFG3_PMDL1WAY       0x10000000 /* Peripheral Module Disable - only 1 reconfig */
#define MX1_CFG3_IOL1WAY        0x20000000 /* Peripheral Pin Select - only 1 reconfig */
#define MX1_CFG3_FUSBIDIO       0x40000000 /* USBID pin: controlled by USB */
#define MX1_CFG3_FVBUSONIO      0x80000000 /* VBuson pin: controlled by USB */

#endif
