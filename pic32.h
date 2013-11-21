/*
 * Microchip PIC32 jtag definitions.
 *
 * Copyright (C) 2011-2013 Serge Vakulenko
 *
 * This file is part of PIC32PROG project, which is distributed
 * under the terms of the GNU General Public License (GPL).
 * See the accompanying file "COPYING" for more details.
 */

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

/*
 * Config0 register, inverted.
 */
#define DEVCFG0_DEBUG_MASK      0x00000003 /* Debugger enable bits */
#define DEVCFG0_DEBUG_DISABLED  0x00000000 /* Debugger disabled */
#define DEVCFG0_DEBUG_ENABLED   0x00000002 /* Debugger enabled */
#define DEVCFG0_ICESEL_MASK     0x00000018 /* Debugger channel select */
#define DEVCFG0_ICESEL_PAIR1    0x00000000 /* Use PGC1/PGD1 */
#define DEVCFG0_ICESEL_PAIR2    0x00000008 /* Use PGC2/PGD2 */
#define DEVCFG0_ICESEL_PAIR3    0x00000010 /* Use PGC3/PGD3 */
#define DEVCFG0_ICESEL_PAIR4    0x00000018 /* Use PGC4/PGD4 */
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
