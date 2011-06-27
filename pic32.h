/*
 * Microchip PIC32 jtag definitions.
 *
 * Copyright (C) 2011 Serge Vakulenko
 *
 * This file is part of BKUNIX project, which is distributed
 * under the terms of the GNU General Public License (GPL).
 * See the accompanying file "COPYING" for more details.
 */

/*
 * Programing executive.
 * Described in PIC32MX Flash Programming Specification.
 */
#define	PIC32_PE_LOADER_LEN 42
#define PIC32_PE_LEN        1240
#define PIC32_PE_VERSION    0x0109

extern const unsigned short pic32_pe_loader[];
extern const unsigned pic32_pe[];

/*
 * TAP instructions.
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
#define ETAP_ADDRESS    8       // Select Address register
#define ETAP_DATA       9       // Select Data register
#define ETAP_CONTROL    10      // Select EJTAG Control register
#define ETAP_EJTAGBOOT  12      // On reset, enter EJTAG break handler
#define ETAP_FASTDATA   14      // Select FastData register

/*
 * MCHP commands.
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
#define MCHP_STATUS_CFGRDY 0x08 // Configuration has been read and
                                // Code-Protect State bit is valid
#define MCHP_STATUS_FCBUSY 0x04 // Flash Controller is Busy (erase is in progress)
#define MCHP_STATUS_FAEN   0x02 // Flash access is enabled
#define MCHP_STATUS_DEVRST 0x01 // Device reset is active

/*
 * Configuration registers.
 */
#define DEVCFG3_ADDR    0x1fc02ff0
#define DEVCFG2_ADDR    0x1fc02ff4
#define DEVCFG1_ADDR    0x1fc02ff8
#define DEVCFG0_ADDR    0x1fc02ffc

/*
 * Config0 register, inverted.
 */
#define DEVCFG0_DEBUG_MASK      0x00000003 /* Debugger enable bits */
#define DEVCFG0_DEBUG_DISABLED  0x00000000 /* Debugger disabled */
#define DEVCFG0_DEBUG_ENABLED   0x00000002 /* Debugger enabled */
#define DEVCFG0_ICESEL          0x00000008 /* Use PGC1/PGD1 (default PGC2/PGD2) */
#define DEVCFG0_PWP_MASK        0x000ff000 /* Program flash write protect */
#define DEVCFG0_BWP             0x01000000 /* Boot flash write protect */
#define DEVCFG0_CP              0x10000000 /* Code protect */

/*
 * Config1 register.
 */
#define DEVCFG1_FNOSC_MASK      0x00000007 /* Oscillator selection */
#define DEVCFG1_FNOSC_FRC       0x00000000 /* Fast RC */
#define DEVCFG1_FNOSC_FRCDIVPLL 0x00000001 /* Fast RC with divide-by-N and PLL */
#define DEVCFG1_FNOSC_PRI       0x00000002 /* Primary oscillator XT, HS, EC */
#define DEVCFG1_FNOSC_PRIPLL    0x00000003 /* Primary with PLL */
#define DEVCFG1_FNOSC_SEC       0x00000004 /* Secondary oscillator */
#define DEVCFG1_FNOSC_LPRC      0x00000005 /* Low-power RC */
#define DEVCFG1_FNOSC_FRCDIV    0x00000007 /* Fast RC with divide-by-N */
#define DEVCFG1_FSOSCEN         0x00000020 /* Secondary oscillator enable */
#define DEVCFG1_IESO            0x00000080 /* Internal-external switch over */
#define DEVCFG1_POSCMOD_MASK    0x00000300 /* Primary oscillator config */
#define DEVCFG1_POSCMOD_EXT     0x00000000 /* External mode */
#define DEVCFG1_POSCMOD_XT      0x00000100 /* XT oscillator */
#define DEVCFG1_POSCMOD_HS      0x00000200 /* HS oscillator */
#define DEVCFG1_POSCMOD_DISABLE 0x00000300 /* Disabled */
#define DEVCFG1_OSCIOFNC        0x00000400 /* CLKO output active */
#define DEVCFG1_FPBDIV_MASK     0x00003000 /* Peripheral bus clock divisor */
#define DEVCFG1_FPBDIV_1        0x00000000 /* SYSCLK / 1 */
#define DEVCFG1_FPBDIV_2        0x00001000 /* SYSCLK / 2 */
#define DEVCFG1_FPBDIV_4        0x00002000 /* SYSCLK / 4 */
#define DEVCFG1_FPBDIV_8        0x00003000 /* SYSCLK / 8 */
#define DEVCFG1_FCKM_DISABLE    0x00004000 /* Fail-safe clock monitor disable */
#define DEVCFG1_FCKS_DISABLE    0x00008000 /* Clock switching disable */
#define DEVCFG1_WDTPS_MASK      0x001f0000 /* Watchdog postscale */
#define DEVCFG1_WDTPS_1         0x00000000 /* 1:1 */
#define DEVCFG1_WDTPS_2         0x00010000 /* 1:2 */
#define DEVCFG1_WDTPS_4         0x00020000 /* 1:4 */
#define DEVCFG1_WDTPS_8         0x00030000 /* 1:8 */
#define DEVCFG1_WDTPS_16        0x00040000 /* 1:16 */
#define DEVCFG1_WDTPS_32        0x00050000 /* 1:32 */
#define DEVCFG1_WDTPS_64        0x00060000 /* 1:64 */
#define DEVCFG1_WDTPS_128       0x00070000 /* 1:128 */
#define DEVCFG1_WDTPS_256       0x00080000 /* 1:256 */
#define DEVCFG1_WDTPS_512       0x00090000 /* 1:512 */
#define DEVCFG1_WDTPS_1024      0x000a0000 /* 1:1024 */
#define DEVCFG1_WDTPS_2048      0x000b0000 /* 1:2048 */
#define DEVCFG1_WDTPS_4096      0x000c0000 /* 1:4096 */
#define DEVCFG1_WDTPS_8192      0x000d0000 /* 1:8192 */
#define DEVCFG1_WDTPS_16384     0x000e0000 /* 1:16384 */
#define DEVCFG1_WDTPS_32768     0x000f0000 /* 1:32768 */
#define DEVCFG1_WDTPS_65536     0x00100000 /* 1:65536 */
#define DEVCFG1_WDTPS_131072    0x00110000 /* 1:131072 */
#define DEVCFG1_WDTPS_262144    0x00120000 /* 1:262144 */
#define DEVCFG1_WDTPS_524288    0x00130000 /* 1:524288 */
#define DEVCFG1_WDTPS_1048576   0x00140000 /* 1:1048576 */
#define DEVCFG1_FWDTEN          0x00800000 /* Watchdog enable */

/*
 * Config2 register.
 */
#define DEVCFG2_FPLLIDIV_MASK   0x00000007 /* PLL input divider */
#define DEVCFG2_FPLLIDIV_1      0x00000000 /* 1x */
#define DEVCFG2_FPLLIDIV_2      0x00000001 /* 2x */
#define DEVCFG2_FPLLIDIV_3      0x00000002 /* 3x */
#define DEVCFG2_FPLLIDIV_4      0x00000003 /* 4x */
#define DEVCFG2_FPLLIDIV_5      0x00000004 /* 5x */
#define DEVCFG2_FPLLIDIV_6      0x00000005 /* 6x */
#define DEVCFG2_FPLLIDIV_10     0x00000006 /* 10x */
#define DEVCFG2_FPLLIDIV_12     0x00000007 /* 12x */
#define DEVCFG2_FPLLMUL_MASK    0x00000070 /* PLL multiplier */
#define DEVCFG2_FPLLMUL_15      0x00000000 /* 15x */
#define DEVCFG2_FPLLMUL_16      0x00000010 /* 16x */
#define DEVCFG2_FPLLMUL_17      0x00000020 /* 17x */
#define DEVCFG2_FPLLMUL_18      0x00000030 /* 18x */
#define DEVCFG2_FPLLMUL_19      0x00000040 /* 19x */
#define DEVCFG2_FPLLMUL_20      0x00000050 /* 20x */
#define DEVCFG2_FPLLMUL_21      0x00000060 /* 21x */
#define DEVCFG2_FPLLMUL_24      0x00000070 /* 24x */
#define DEVCFG2_UPLLIDIV_MASK   0x00000700 /* USB PLL input divider */
#define DEVCFG2_UPLLIDIV_1      0x00000000 /* 1x */
#define DEVCFG2_UPLLIDIV_2      0x00000100 /* 2x */
#define DEVCFG2_UPLLIDIV_3      0x00000200 /* 3x */
#define DEVCFG2_UPLLIDIV_4      0x00000300 /* 4x */
#define DEVCFG2_UPLLIDIV_5      0x00000400 /* 5x */
#define DEVCFG2_UPLLIDIV_6      0x00000500 /* 6x */
#define DEVCFG2_UPLLIDIV_10     0x00000600 /* 10x */
#define DEVCFG2_UPLLIDIV_12     0x00000700 /* 12x */
#define DEVCFG2_UPLLDIS         0x00008000 /* Disable USB PLL */
#define DEVCFG2_FPLLODIV_MASK   0x00070000 /* Default postscaler for PLL */
#define DEVCFG2_FPLLODIV_1      0x00000000 /* 1x */
#define DEVCFG2_FPLLODIV_2      0x00010000 /* 2x */
#define DEVCFG2_FPLLODIV_4      0x00020000 /* 4x */
#define DEVCFG2_FPLLODIV_8      0x00030000 /* 8x */
#define DEVCFG2_FPLLODIV_16     0x00040000 /* 16x */
#define DEVCFG2_FPLLODIV_32     0x00050000 /* 32x */
#define DEVCFG2_FPLLODIV_64     0x00060000 /* 64x */
#define DEVCFG2_FPLLODIV_256    0x00070000 /* 256x */

/*
 * Config3 register.
 */
#define DEVCFG3_USERID_MASK     0x0000ffff /* User-defined ID */
#define DEVCFG3_FSRSSEL_MASK    0x00070000 /* SRS select */
#define DEVCFG3_FSRSSEL_ALL     0x00000000 /* All irqs assigned to shadow set */
#define DEVCFG3_FSRSSEL_1       0x00010000 /* Assign irq priority 1 to shadow set */
#define DEVCFG3_FSRSSEL_2       0x00020000 /* Assign irq priority 2 to shadow set */
#define DEVCFG3_FSRSSEL_3       0x00030000 /* Assign irq priority 3 to shadow set */
#define DEVCFG3_FSRSSEL_4       0x00040000 /* Assign irq priority 4 to shadow set */
#define DEVCFG3_FSRSSEL_5       0x00050000 /* Assign irq priority 5 to shadow set */
#define DEVCFG3_FSRSSEL_6       0x00060000 /* Assign irq priority 6 to shadow set */
#define DEVCFG3_FSRSSEL_7       0x00070000 /* Assign irq priority 7 to shadow set */
#define DEVCFG3_FMIIEN          0x01000000 /* Ethernet MII enable */
#define DEVCFG3_FETHIO          0x02000000 /* Ethernet pins default */
#define DEVCFG3_FCANIO          0x04000000 /* CAN pins default */
#define DEVCFG3_FUSBIDIO        0x40000000 /* USBID pin: controlled by USB */
#define DEVCFG3_FVBUSONIO       0x80000000 /* VBuson pin: controlled by USB */
