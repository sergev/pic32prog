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
#define PIC32_PE_LOADER_LEN    42
#define PIC32_PEMM_LOADER_LEN  28

extern const unsigned short pic32_pe_loader[];
extern const unsigned short pic32_pemm_loader[];
extern const unsigned pic32_pemx1[];
extern const unsigned pic32_pemx3[];
extern const unsigned pic32_pemz[];
extern const unsigned pic32_pemm_gpl[];
extern const unsigned pic32_pemm_gpm[];
extern const unsigned pic32_pemk[];

#define FAMILY_MX1	0
#define FAMILY_MX3	1
#define FAMILY_MZ	2
#define FAMILY_MK	3
#define FAMILY_MM	4

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
 * Length of TAP/MTAP and ETAP commands
 */
#define ETAP_COMMAND_NBITS		5
#define MTAP_COMMAND_NBITS		5
#define MTAP_COMMAND_DR_NBITS	8


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
#define PE_QUAD_WORD_PGRM       0xD     /* Programs four words of Flash memory at the specified address */
#define PE_DOUBLE_WORD_PGRM     0xE     /* Programs two words of Flash memory at the specified address */

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

/*-------------------------------------------------------------------
 * MM family.
 *
 * FDEVOPT register
 */
#define MM_FDEVOPT_USERID_MASK   0xffff0000 /* User-defined ID */
#define MM_FDEVOPT_FVBUSIO       0x00008000 /* FVBUSIO bit */
#define MM_FDEVOPT_FUSBIDIO      0x00004000 /* FUSBIDIO bit */
#define MM_FDEVOPT_ALTI2C        0x00000010 /* ALTI2C bit */
#define MM_FDEVOPT_SOSCHP        0x00000008 /* SOSCHP bit */

/*
 * FICD register.
 */
#define MM_FICD_ICS_MASK        0x00000018 /* ICS mask for both bits */
#define MM_FICD_ICS_PAIR1       0x00000018 /* Use PGC1/PGD1 */
#define MM_FICD_ICS_PAIR2       0x00000010 /* Use PGC2/PGD2 */
#define MM_FICD_ICS_PAIR3       0x00000008 /* Use PGC3/PGD3 */
#define MM_FICD_ICS_PAIRNONE    0x00000000 /* Not connected */
#define MM_FICD_JTAGEN          0x00000004 /* JTAGEN bit */ 

/*
 * FPOR register.
 */
#define MM_FPOR_LPBOREN         0x00000008 /* LPBOREN bit */
#define MM_FPOR_RETVR           0x00000004 /* RETVR bit */
#define MM_FPOR_BOREN_MASK      0x00000003 /* Mask for BOREN */
#define MM_FPOR_BOREN3          0x00000003 /* Brown-out Reset enabled in HW, SBOREN bit is disabled */
#define MM_FPOR_BOREN2          0x00000002 /* Brown-out Reset in enabled only while device is active and disabled in Sleep; SBOREN bit is disabled */
#define MM_FPOR_BOREN1          0x00000001 /* Brown-out Reset is controlled with the SBOREN bit setting */
#define MM_FPOR_BOREN0          0x00000000 /* Brown-out Reset is disabled in HWM SBOREN bit is disabled */

/*
 * FWDT register.
 */
#define MM_FWDT_FWDTEN          0x00008000 /* FWDTEN bit */
#define MM_FWDT_RCLKSEL_MASK    0x00006000 /* Mask for RCLKSEL */
#define MM_FWDT_RCLKSEL_LPRC    0x00006000 /* Clock source is LPRC, same as for Sleep mode */
#define MM_FWDT_RCLKSEL_FRC     0x00004000 /* Clock source is the FRC oscillator */
#define MM_FWDT_RCLKSEL_RES     0x00002000 /* Reserved */
#define MM_FWDT_RCLKSEL_SYS     0x00000000 /* Clock source is the system clock */
#define MM_FWDT_RWDTPS_MASK     0x00001F00 /* Run Mode Watchdog Timer Postscale Select bits */
#define MM_FWDT_RWDTPS_1        0x00000000 /* 1:1 */
#define MM_FWDT_RWDTPS_2        0x00000100 /* 1:2 */
#define MM_FWDT_RWDTPS_4        0x00000200 /* 1:4 */
#define MM_FWDT_RWDTPS_8        0x00000300 /* 1:8 */
#define MM_FWDT_RWDTPS_16       0x00000400 /* 1:16 */
#define MM_FWDT_RWDTPS_32       0x00000500 /* 1:32 */
#define MM_FWDT_RWDTPS_64       0x00000600 /* 1:64 */
#define MM_FWDT_RWDTPS_128      0x00000700 /* 1:128 */
#define MM_FWDT_RWDTPS_256      0x00000800 /* 1:256 */
#define MM_FWDT_RWDTPS_512      0x00000900 /* 1:512 */
#define MM_FWDT_RWDTPS_1024     0x00000A00 /* 1:1024 */
#define MM_FWDT_RWDTPS_2048     0x00000B00 /* 1:2048 */
#define MM_FWDT_RWDTPS_4096     0x00000C00 /* 1:4096 */
#define MM_FWDT_RWDTPS_8192     0x00000D00 /* 1:8192 */
#define MM_FWDT_RWDTPS_16384    0x00000E00 /* 1:16384 */
#define MM_FWDT_RWDTPS_32768    0x00000F00 /* 1:32768 */
#define MM_FWDT_RWDTPS_65536    0x00001000 /* 1:65536 */
#define MM_FWDT_RWDTPS_131072   0x00001100 /* 1:131072 */
#define MM_FWDT_RWDTPS_262144   0x00001200 /* 1:262144 */
#define MM_FWDT_RWDTPS_524288   0x00001300 /* 1:524288 */
#define MM_FWDT_RWDTPS_1048576  0x00001400 /* 1:1048573, from this to 0x1F00 */
#define MM_FWDT_WINDIS          0x00000080 /* WINDIS bit */
#define MM_FWDT_FWDTWINSZ_MASK  0x00000060 /* FWDTWINSZ mask */
#define MM_FWDT_FWDTWINSZ_25    0x00000060 /* Watchdog window size 25% */
#define MM_FWDT_FWDTWINSZ_375   0x00000040 /* Watchdog window size 37.5% */
#define MM_FWDT_FWDTWINSZ_50    0x00000020 /* Watchdog window size 50% */
#define MM_FWDT_FWDTWINSZ_75    0x00000000 /* Watchdog window size 75% */
#define MM_FWDT_SWDTPS_MASK     0x0000001F /* Sleep Mode Watchdog Timer Postscale Select bits */
#define MM_FWDT_SWDTPS_1        0x00000000 /* 1:1 */
#define MM_FWDT_SWDTPS_2        0x00000001 /* 1:2 */
#define MM_FWDT_SWDTPS_4        0x00000002 /* 1:4 */
#define MM_FWDT_SWDTPS_8        0x00000003 /* 1:8 */
#define MM_FWDT_SWDTPS_16       0x00000004 /* 1:16 */
#define MM_FWDT_SWDTPS_32       0x00000005 /* 1:32 */
#define MM_FWDT_SWDTPS_64       0x00000006 /* 1:64 */
#define MM_FWDT_SWDTPS_128      0x00000007 /* 1:128 */
#define MM_FWDT_SWDTPS_256      0x00000008 /* 1:256 */
#define MM_FWDT_SWDTPS_512      0x00000009 /* 1:512 */
#define MM_FWDT_SWDTPS_1024     0x0000000A /* 1:1024 */
#define MM_FWDT_SWDTPS_2048     0x0000000B /* 1:2048 */
#define MM_FWDT_SWDTPS_4096     0x0000000C /* 1:4096 */
#define MM_FWDT_SWDTPS_8192     0x0000000D /* 1:8192 */
#define MM_FWDT_SWDTPS_16384    0x0000000E /* 1:16384 */
#define MM_FWDT_SWDTPS_32768    0x0000000F /* 1:32768 */
#define MM_FWDT_SWDTPS_65536    0x00000010 /* 1:65536 */
#define MM_FWDT_SWDTPS_131072   0x00000011 /* 1:131072 */
#define MM_FWDT_SWDTPS_262144   0x00000012 /* 1:262144 */
#define MM_FWDT_SWDTPS_524288   0x00000013 /* 1:524288 */
#define MM_FWDT_SWDTPS_1048576  0x00000014 /* 1:1048573, from this to 0x001F */

/*
 * FOSCSEL register.
 */
#define MM_FOSCSEL_FCKSM_MASK   0x0000C000 /* Mask for FCKSM  - clock switching and Fail-Safe Clock Monitor Enable bits */
#define MM_FOSCSEL_FCKSM3       0x0000C000 /* Clock is enabled, Fail-Safe Clock Monitor is enabled */
#define MM_FOSCSEL_FCKSM2       0x00008000 /* Clock is disabled, Fail-Safe Clock Monitor is enabled */
#define MM_FOSCSEL_FCKSM1       0x00004000 /* Clock is enabled, Fail-Safe Clock Monitor is disabled */
#define MM_FOSCSEL_FCKSM0       0x00000000 /* Clock is disabled, Fail-Safe Clock Monitor is disabled */
#define MM_FOSCSEL_SOSCSEL      0x00001000 /* Secondary oscillator enable bit */
#define MM_FOSCSEL_OSCIOFNC     0x00000400 /* System clock on CLKO Pin enable bit */
#define MM_FOSCSEL_POSCMOD_MASK 0x00000300 /* Primary oscillator mode select bit */
#define MM_FOSCSEL_POSCMOD_DIS  0x00000300 /* Primary oscillator is disabled */
#define MM_FOSCSEL_POSCMOD_HS   0x00000200 /* HS oscillator mode selected */
#define MM_FOSCSEL_POSCMOD_XT   0x00000100 /* XT oscillator mode selected */
#define MM_FOSCSEL_POSCMOD_EC   0x00000000 /* EC oscillator mode selected */
#define MM_FOSCSEL_IESO         0x00000080 /* Two-speed start-up enable bit */
#define MM_FOSCSEL_SOSCEN       0x00000040 /* Secondary oscillator enable bit */
#define MM_FOSCSEL_PLLSRC       0x00000010 /* System PLL Input selection bit */
#define MM_FOSCSEL_FNOSC_MASK   0x00000006 /* Oscillator selection bits */
#define MM_FOSCSEL_FNOSC_PRIM_FRC_PLL 0x00000001 /* Primary or FRC oscillator + PLL */
#define MM_FOSCSEL_FNOSC_PRIM         0x00000002 /* Primary oscillator (XT, HS, EC) */
#define MM_FOSCSEL_FNOSC_RESERVED     0x00000003 /* Reserved */
#define MM_FOSCSEL_FNOSC_SOCS         0x00000004 /* Secondary oscillator */
#define MM_FOSCSEL_FNOSC_LPRC         0x00000005 /* Low-power RC oscillator */
// Everything else, 0, 0x06, 0x07 is FRC + Divide-by-N
#define MM_FOSCSEL_FNOSC_FRC_DIVN1    0x00000000
#define MM_FOSCSEL_FNOSC_FRC_DIVN6    0x00000006
#define MM_FOSCSEL_FNOSC_FRC_DIVN7    0x00000007

/*
 * FSEC register.
 */
#define MM_FSEC_CP              0x80000000 /* Code protect bit */


/*-------------------------------------------------------------------
 * MK family.
 *
 * DEVCFG3 register
 */
#define MK_DEVCFG3_FVBUSIO1		0x80000000 /* VBUSON pin controlled by USB1 */
#define MK_DEVCFG3_FUSBIDIO1	0x40000000 /* USBID pin controlled by USB1 */
#define MK_DEVCFG3_IOL1WAY		0x20000000 /* Peripheral Pin select - only 1 reconfig */
#define MK_DEVCFG3_PMDL1WAY		0x10000000 /* Peripheral module disable - only 1 reconfig */
#define MK_DEVCFG3_PGL1WAY		0x08000000 /* Permission group lock - only 1 reconfig */
#define MK_DEVCFG3_FVBUSIO2		0x00800000 /* VBUSON pin controlled by USB2 */
#define MK_DEVCFG3_FUSBIDIO2	0x00400000 /* USBID pin controlled by USB2 */
#define MK_DEVCFG3_PWMLOCK		0x00100000 /* Write access to PWM IOCONx not protected */
#define MK_DEVCFG3_USERID_MASK  0x0000FFFF /* User-defined ID */

/*
 * DEVCFG2 register.
 */
#define MK_DEVCFG2_UPLLEN		0x80000000 /* USB PLL disabled */
#define MK_DEVCFG2_BORSEL		0x20000000 /* BOR trip voltage 2.1V, non-opamp config */
#define MK_DEVCFG2_FDSEN		0x10000000 /* DS bit enable on WAIT command */
#define MK_DEVCFG2_DSWDTEN		0x08000000 /* Enable DSWDTEN during Deep Sleep */
#define MK_DEVCFG2_DSWDTOSC		0x04000000 /* Select LPRC as DSWDT reference clock */
#define MK_DEVCFG2_DSWDTPS_MASK	0x03E00000 /* Deep Sleep Watchdog Timer Postscale Select bits */
#define MK_DEVCFG2_DSWDTPS_236	0x03E00000 /* 1:236 (25.7 days), these values are 2^XY*/
#define MK_DEVCFG2_DSWDTPS_235	0x03C00000 /* 1:235 (12.8 days) */
#define MK_DEVCFG2_DSWDTPS_234	0x03A00000 /* 1:234 (6.4 days) */
#define MK_DEVCFG2_DSWDTPS_233	0x03800000 /* 1:233 (77.0 hours) */ 
#define MK_DEVCFG2_DSWDTPS_232	0x03600000 /* 1:232 (38.5 hours) */ 
#define MK_DEVCFG2_DSWDTPS_231	0x03400000 /* 1:231 (19.2 hours) */ 
#define MK_DEVCFG2_DSWDTPS_230	0x03200000 /* 1:230 (9.6 hours) */ 
#define MK_DEVCFG2_DSWDTPS_229	0x03000000 /* 1:229 (4.8 hours) */ 
#define MK_DEVCFG2_DSWDTPS_228	0x02E00000 /* 1:228 (2.4 hours) */ 
#define MK_DEVCFG2_DSWDTPS_227	0x02C00000 /* 1:227 (72.2 minutes) */ 
#define MK_DEVCFG2_DSWDTPS_226	0x02A00000 /* 1:226 (36.1 minutes) */ 
#define MK_DEVCFG2_DSWDTPS_225	0x02800000 /* 1:225 (18.0 minutes) */ 
#define MK_DEVCFG2_DSWDTPS_224	0x02600000 /* 1:224 (9.0 minutes) */ 
#define MK_DEVCFG2_DSWDTPS_223	0x02400000 /* 1:223 (4.5 minutes) */ 
#define MK_DEVCFG2_DSWDTPS_222	0x02200000 /* 1:222 (135.5 seconds) */ 
#define MK_DEVCFG2_DSWDTPS_221	0x02000000 /* 1:221 (67.7 seconds) */ 
#define MK_DEVCFG2_DSWDTPS_220	0x01E00000 /* 1:220 (33.825 seconds) */ 
#define MK_DEVCFG2_DSWDTPS_219	0x01C00000 /* 1:219 (16.912 seconds) */ 
#define MK_DEVCFG2_DSWDTPS_218	0x01A00000 /* 1:218 (8.456 seconds) */ 
#define MK_DEVCFG2_DSWDTPS_217	0x01800000 /* 1:217 (4.228 seconds) */ 
#define MK_DEVCFG2_DSWDTPS_65536	0x01600000 /* 1:65536 (2.114 seconds) */ 
#define MK_DEVCFG2_DSWDTPS_32768	0x01400000 /* 1:32768 (1.057 seconds) */ 
#define MK_DEVCFG2_DSWDTPS_16384	0x01200000 /* 1:16384 (528.5 milliseconds) */ 
#define MK_DEVCFG2_DSWDTPS_8192	0x01000000 /* 1:8192 (264.3 milliseconds) */ 
#define MK_DEVCFG2_DSWDTPS_4096	0x00E00000 /* 1:4096 (132.1 milliseconds) */ 
#define MK_DEVCFG2_DSWDTPS_2048	0x00C00000 /* 1:2048 (66.1 milliseconds) */ 
#define MK_DEVCFG2_DSWDTPS_1024	0x00A00000 /* 1:1024 (33 milliseconds) */ 
#define MK_DEVCFG2_DSWDTPS_512	0x00800000 /* 1:512 (16.5 milliseconds) */ 
#define MK_DEVCFG2_DSWDTPS_256	0x00600000 /* 1:256 (8.3 milliseconds) */ 
#define MK_DEVCFG2_DSWDTPS_128	0x00400000 /* 1:128 (4.1 milliseconds) */ 
#define MK_DEVCFG2_DSWDTPS_64	0x00200000 /* 1:64 (2.1 milliseconds) */ 
#define MK_DEVCFG2_DSWDTPS_32	0x00000000 /* 1:32 (1 millisecond) */ 
#define MK_DEVCFG2_DSBOREN		0x00100000 /* Enable ZPBOR during deep sleep */
#define MK_DEVCFG2_VBATBOREN	0x00080000 /* Enable ZPBOR during VBAT mode */
#define MK_DEVCFG2_FPLLODIV_MASK	0x00070000 /* Mask for System PLL Output Divisor bits */
#define MK_DEVCFG2_FPLLODIV_32_1	0x00070000 /* Output divided by 32 */
#define MK_DEVCFG2_FPLLODIV_32_2	0x00060000 /* Output divided by 32 */
#define MK_DEVCFG2_FPLLODIV_32_3	0x00050000 /* Output divided by 32 */
#define MK_DEVCFG2_FPLLODIV_16	0x00040000 /* Output divided by 16 */
#define MK_DEVCFG2_FPLLODIV_8	0x00030000 /* Output divided by 8 */
#define MK_DEVCFG2_FPLLODIV_4	0x00020000 /* Output divided by 4 */
#define MK_DEVCFG2_FPLLODIV_2_1	0x00010000 /* Output divided by 2 */
#define MK_DEVCFG2_FPLLODIV_2_2	0x00000000 /* Output divided by 2 */
#define MK_DEVCFG2_FPLLMULT_MASK	0x00007F00	/* Mask for System PLL Feedback Divider bits */
#define MK_DEVCFG2_FPLLMULT_SHIFT	8			/* By how much to shift the value to the right */
#define MK_DEVCFG2_FPLLMULT_MIN_VAL	1			/* Value of 0000000 in the register. To many to do manually */
#define MK_DEVCFG2_FPLLICLK		0x00000080 /* FRC is selected as input to the System PLL */
#define MK_DEVCFG2_FPLLRNG_MASK	0x00000070 /* System PLL Divided Input Clock Frequency Range */
#define MK_DEVCFG2_FPLLRNG_34_64	0x00000050 /* 34-64 MHz */
#define MK_DEVCFG2_FPLLRNG_21_42	0x00000040 /* 21-42 MHz */
#define MK_DEVCFG2_FPLLRNG_13_26	0x00000030 /* 13-26 MHz */
#define MK_DEVCFG2_FPLLRNG_8_16		0x00000020 /* 8-16 MHz */
#define MK_DEVCFG2_FPLLRNG_5_10		0x00000010 /* 5-10 MHz */
#define MK_DEVCFG2_FPLLRNG_BYPASS	0x00000000 /* Bypass */
/* Two other values are Reserved */
#define MK_DEVCFG2_FPLLIDIV_MASK	0x00000007 /* Mask for PLL Input Divider bits */
#define MK_DEVCFG2_FPLLIDIV_8		0x00000007 /* Divide by 8 */
#define MK_DEVCFG2_FPLLIDIV_7		0x00000006 /* Divide by 7 */
#define MK_DEVCFG2_FPLLIDIV_6		0x00000005 /* Divide by 6 */
#define MK_DEVCFG2_FPLLIDIV_5		0x00000004 /* Divide by 5 */
#define MK_DEVCFG2_FPLLIDIV_4		0x00000003 /* Divide by 4 */
#define MK_DEVCFG2_FPLLIDIV_3		0x00000002 /* Divide by 3 */
#define MK_DEVCFG2_FPLLIDIV_2		0x00000001 /* Divide by 2 */
#define MK_DEVCFG2_FPLLIDIV_1		0x00000000 /* Divide by 1 */

/*
 * DEVCFG1 register.
 */
#define MK_DEVCFG1_FDMTEN		0x80000000 /* Deadman Timer is enabled and canot be disabled by software */
#define MK_DEVCFG1_DMTCNT_MASK	0x7C000000 /* Mask for Deadman Timer Count Select bits */
#define MK_DEVCFG1_DMTCNT_SHIFT	26 /* By how much to shift the value to the right */
#define MK_DEVCFG1_DMTCNT_MIN_EXPONENT	8	/* 2^8, or 256 */
#define MK_DEVCFG1_DMTCNT_MAX_EXPONENT	31	/* 2^31, or 2147483648 */
#define MK_DEVCFG1_FWDTWINSZ_MASK	0x03000000 /* Mask for Watchdog Timer Windows Size bits */
#define MK_DEVCFG1_FWDTWINSZ_25		0x03000000 /* Window size is 25% */
#define MK_DEVCFG1_FWDTWINSZ_27_5	0x02000000 /* Window size is 37.5% */
#define MK_DEVCFG1_FWDTWINSZ_50		0x01000000 /* Window size is 50% */
#define MK_DEVCFG1_FWDTWINSZ_75		0x00000000 /* Window size is 75% */
#define MK_DEVCFG1_FWDTEN			0x00800000 /* Watchdog Timer is enabled and cannot be disabled by software */
#define MK_DEVCFG1_WINDIS			0x00400000 /* Watchdog Times is in non-Windows mode */
#define MK_DEVCFG1_WDTSPGM			0x00200000 /* Watchdog Timer stops during Flash programming */
#define MK_DEVCFG1_WDTPS_MASK		0x001F0000 /* Watchdog Timer Postscale Select bits */
#define MK_DEVCFG1_WDTPS_1048576	0x00140000 /* 1:1048576 */
#define MK_DEVCFG1_WDTPS_524288		0x00130000 /* 1:524288 */
#define MK_DEVCFG1_WDTPS_262144		0x00120000 /* 1:262144 */
#define MK_DEVCFG1_WDTPS_131072		0x00110000 /* 1:131072 */
#define MK_DEVCFG1_WDTPS_65536		0x00100000 /* 1:65536 */
#define MK_DEVCFG1_WDTPS_32768		0x000F0000 /* 1:32768 */
#define MK_DEVCFG1_WDTPS_16384		0x000E0000 /* 1:16384 */
#define MK_DEVCFG1_WDTPS_8192		0x000D0000 /* 1:8192 */
#define MK_DEVCFG1_WDTPS_4096		0x000C0000 /* 1:4096 */
#define MK_DEVCFG1_WDTPS_2048		0x000B0000 /* 1:2048 */
#define MK_DEVCFG1_WDTPS_1024		0x000A0000 /* 1:1024 */
#define MK_DEVCFG1_WDTPS_512		0x00090000 /* 1:512 */
#define MK_DEVCFG1_WDTPS_256		0x00080000 /* 1:256 */
#define MK_DEVCFG1_WDTPS_128		0x00070000 /* 1:128 */
#define MK_DEVCFG1_WDTPS_64			0x00060000 /* 1:64 */
#define MK_DEVCFG1_WDTPS_32			0x00050000 /* 1:32 */
#define MK_DEVCFG1_WDTPS_16			0x00040000 /* 1:16 */
#define MK_DEVCFG1_WDTPS_8			0x00030000 /* 1:8 */
#define MK_DEVCFG1_WDTPS_4			0x00020000 /* 1:4 */
#define MK_DEVCFG1_WDTPS_2			0x00010000 /* 1:2 */
#define MK_DEVCFG1_WDTPS_1			0x00000000 /* 1:1 */
#define MK_DEVCFG1_FCKSM_MASK		0x0000C000 /* Clock Switching and Monitoring Selection mask */
#define MK_DEVCFG1_FCKSM_3			0x0000C000 /* Clock switching enabled, clock monitoring enabled */
#define MK_DEVCFG1_FCKSM_2			0x00008000 /* Clock switching disabled, clock monitoring enabled */
#define MK_DEVCFG1_FCKSM_1			0x00004000 /* Clock switching enabled, clock monitoring disabled */
#define MK_DEVCFG1_FCKSM_0			0x00000000 /* Clock switching disabled, clock monitoring disabled */
#define MK_DEVCFG1_OSCIOFNC			0x00000400 /* CLKO output is disabled */
#define MK_DEVCFG1_POSCMOD_MASK		0x00000300 /* Primary Oscillator Configuration mask */
#define MK_DEVCFG1_POSCMOD_DISABLED	0x00000300 /* Posc is disabled */
#define MK_DEVCFG1_POSCMOD_HS		0x00000200 /* HS Oscillator mode is selected */
#define MK_DEVCFG1_POSCMOD_RESERVED	0x00000100 /* Reserved */
#define MK_DEVCFG1_POSCMOD_EC		0x00000000 /* EX mode is selected */
#define MK_DEVCFG1_IESO			0x00000080 /* Internal External Switchoved mode enabled, Two-Speed Startup enabled */
#define MK_DEVCFG1_FSOSCEN		0x00000040 /* Enable Sosc (if using osillator instead of crystal, bit should be 0) */
#define MK_DEVCFG1_DMTINV_MASK	0x00000038 /* Deadman Timer Count Window Interval mask */
#define MK_DEVCFG1_DMTINV_127_128	0x00000038 /* Windows is 127/128 counter value */
#define MK_DEVCFG1_DMTINV_63_64		0x00000030 /* Windows is 63/64 counter value */
#define MK_DEVCFG1_DMTINV_31_32		0x00000028 /* Windows is 31/32 counter value */
#define MK_DEVCFG1_DMTINV_15_16		0x00000020 /* Windows is 15/16 counter value */
#define MK_DEVCFG1_DMTINV_7_8		0x00000018 /* Windows is 7/8 counter value */
#define MK_DEVCFG1_DMTINV_3_4		0x00000010 /* Windows is 3/4 counter value */
#define MK_DEVCFG1_DMTINV_1_2		0x00000008 /* Windows is 1/2 counter value */
#define MK_DEVCFG1_DMTINV_0			0x00000000 /* Windows value is zero */
#define MK_DEVCFG1_FNOSC_MASK		0x00000007 /* Oscillator selection bits */
#define MK_DEVCFG1_FNOSC_LPRC		0x00000005 /* Low-power RC Oscillator (LPRC) */
#define MK_DEVCFG1_FNOSC_SOSC		0x00000004 /* Secondary Oscillator */
#define MK_DEVCFG1_FNOSC_USBPLL		0x00000003 /* USB PLL - UPLL Module, set by UPLLCON */
#define MK_DEVCFG1_FNOSC_POSC		0x00000002 /* Primary oscillator (Posc) - HS, EC */
#define MK_DEVCFG1_FNOSC_SYSTEMPLL	0x00000001 /* System PLL - SPLL Module, set by SPLLCON */
#define MK_DEVCFG1_FNOSC_FRC		0x00000000 /* Fast RC Oscillator + divider by FRCDIV (1,2,4,8,16,32,64,256) */

/*
 * DEVCFG0 register.
 */
#define MK_DEVCFG0_EJTAGBEN			0x40000000 /* Normal EJTAG functionality */
#define MK_DEVCFG0_POSCBOOST		0x00200000 /* POSC, Boost the kick start of the oscillator */
#define MK_DEVCFG0_POSCGAIN_MASK	0x00180000 /* Primary oscillator gain control */
#define MK_DEVCFG0_POSCGAIN_3		0x00180000 /* Gain level 3 (highest) */
#define MK_DEVCFG0_POSCGAIN_2		0x00100000 /* Gain level 2 */
#define MK_DEVCFG0_POSCGAIN_1		0x00080000 /* Gain level 1 */
#define MK_DEVCFG0_POSCGAIN_0		0x00000000 /* Gain level 0 (lowest) */
#define MK_DEVCFG0_SOSCBOOST		0x00040000 /* SOSC, Boost the kisk start of the oscillator */
#define MK_DEVCFG0_SOSCGAIN_MASK	0x00030000 /* Secondary oscillator gain control */
#define MK_DEVCFG0_SOSCGAIN_3		0x00030000 /* Gain level 3 (highest) */
#define MK_DEVCFG0_SOSCGAIN_2		0x00020000 /* Gain level 2 */
#define MK_DEVCFG0_SOSCGAIN_1		0x00010000 /* Gain level 1 */
#define MK_DEVCFG0_SOSCGAIN_0		0x00000000 /* Gain level 0 (lowest) */
#define MK_DEVCFG0_SMCLR			0x00008000 /* MCLR pin generates a normal system Reset */
#define MK_DEVCFG0_DBGPER_MASK		0x00007000 /* Debug mode CPU Access Permissions mask */
#define MK_DEVCFG0_DBGPER_GRP2		0x00004000 /* Allow/deny access to Permission Group 2 regions */
#define MK_DEVCFG0_DBGPER_GRP1		0x00002000 /* Allow/deny access to Permission Group 1 regions */
#define MK_DEVCFG0_DBGPER_GRP0		0x00001000 /* Allow/deny access to Permission Group 0 regions */
#define MK_DEVCFG0_SPECIALDEBUGGERBIT	0x00000800 /* Only used by debuggers/emulators. Not used otherwise */
#define MK_DEVCFG0_FSLEEP			0x00000400 /* Flash is powered down then device is in sleep mode */
#define MK_DEVCFG0_BOOTISA			0x00000040 /* Boot code and Exception code is MIPS32 */
#define MK_DEVCFG0_TRCEN			0x00000020 /* Trace features in the CPU are enabled */
#define MK_DEVCFG0_ICESEL_MASK		0x00000018 /* ICSP Channel Select mask */
#define MK_DEVCFG0_ICESEL_1			0x00000018 /* PGEC1/PGED1 is used */
#define MK_DEVCFG0_ICESEL_2			0x00000010 /* PGEC2/PGED2 is used */
#define MK_DEVCFG0_ICESEL_3			0x00000008 /* PGEC3/PGED3 is used */
#define MK_DEVCFG0_ICESEL_RESERVED	0x00000000 /* Reserved */
#define MK_DEVCFG0_JTAGEN			0x00000004 /* JTAG is enabled */
#define MK_DEFCFG0_DEBUG_MASK		0x00000003 /* Background debugger mask */
#define MK_DEFCFG0_DEBUG_3			0x00000003 /* 4-wire JTAG enabled, ICSP disabled, ICD module disabled */
#define MK_DEFCFG0_DEBUG_2			0x00000002 /* 4-wire JTAG enabled, ICSP disabled, ICD module enabled */
#define MK_DEFCFG0_DEBUG_1			0x00000001 /* 4-wire JTAG disabled, ICSP enabled, ICD module disabled */
#define MK_DEFCFG0_DEBUG_0			0x00000000 /* 4-wire JTAG disabled, ICSP enabled, ICD module enabled */

/*
 * DEVCP register.
 */
#define MK_DEVCP0_CP				0x10000000 /* Protection disabled */

/*
 * DEVSIGN register.
 * Nothing really to do?
 */



#endif









