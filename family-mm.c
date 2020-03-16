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
#include <stdint.h>

#define PRIMARY 0
#define ALTERNATE 1

void print_mm_fdevopt(uint32_t fdevopt, uint32_t alternate);
void print_mm_ficd(uint32_t ficd, uint32_t alternate);
void print_mm_fpor(uint32_t fpor, uint32_t alternate);
void print_mm_fwdt(uint32_t fwdt, uint32_t alternate);
void print_mm_foscsel(uint32_t foscsel, uint32_t alternate);
void print_mm_fsec(uint32_t fsec, uint32_t alternate);

/*
 * Print configuration for MM family.
 */
void print_mm(unsigned cfg0, unsigned cfg1, unsigned cfg2, unsigned cfg3,
				unsigned cfg4, unsigned cfg5, unsigned cfg6, unsigned cfg7,
				unsigned cfg8, unsigned cfg9, unsigned cfg10, unsigned cfg11,
				unsigned cfg12, unsigned cfg13, unsigned cfg14, unsigned cfg15,
				unsigned cfg16, unsigned cfg17)
{
	printf("Primary configuration bits\n");
	print_mm_fdevopt(cfg0, PRIMARY);
	print_mm_ficd(cfg1, PRIMARY);
	print_mm_fpor(cfg2, PRIMARY);
	print_mm_fwdt(cfg3, PRIMARY);
	print_mm_foscsel(cfg4, PRIMARY);
	print_mm_fsec(cfg5, PRIMARY);

	printf("\n");

	printf("Alternative configuration bits\n");
	print_mm_fdevopt(cfg6, ALTERNATE);
	print_mm_ficd(cfg7, ALTERNATE);
	print_mm_fpor(cfg8, ALTERNATE);
	print_mm_fwdt(cfg9, ALTERNATE);
	print_mm_foscsel(cfg10, ALTERNATE);
	print_mm_fsec(cfg11, ALTERNATE);

	printf("\n");

}

// When printing values, shift so the value fits into the first 4 bits.
// That way, can align it under each nibble.
// If there's more bits, shift so it fits into 8 bits, and display as %02x

// ALL values are in hex.

void print_mm_fdevopt(uint32_t fdevopt, uint32_t alternate){
	// FDEVOPT
	if (PRIMARY == alternate){
		printf("    FDEVOPT = 0x%08X\n", fdevopt);
	}
	else{
		printf("   AFDEVOPT = 0x%08X\n", fdevopt);
	}
	printf("              0x%04X     USERID\n", (fdevopt & MM_FDEVOPT_USERID_MASK) >> 16);
	if (fdevopt & MM_FDEVOPT_FVBUSIO){
		printf("                    %01x    VBUSON pin: controlled by USB (GPM series only)\n", MM_FDEVOPT_FVBUSIO >> 12);
	}
	else{
		printf("                    %01x    VBUSON pin: controlled by port function (GPM series only)\n", 0);
	}

	if (fdevopt & MM_FDEVOPT_FUSBIDIO){
		printf("                    %01x    USBID pin: controlled by USB (GPM series only)\n", MM_FDEVOPT_FUSBIDIO >> 12);
	}
	else{
		printf("                    %01x    USBID pin: controlled by port function (GPM series only)\n", 0);
	}

	if (fdevopt & MM_FDEVOPT_ALTI2C){
		printf("                      %01x  I2C1 is on pins RB8 & RB9 (GPM series only)\n", MM_FDEVOPT_ALTI2C >> 4);
	}
	else{
		printf("                      %01x  I2C1 is on alt. pins, RB5 & RC9 (GPM series only)\n", 0);
	}
	if (fdevopt & MM_FDEVOPT_SOSCHP){
		printf("                       %01x SOSC normal power mode\n", MM_FDEVOPT_SOSCHP);
	}
	else{
		printf("                       %01x SOSC High power mode\n", 0);
	}
}

void print_mm_ficd(uint32_t ficd, uint32_t alternate){
	// FICD register
	if (PRIMARY == alternate){
		printf("    FICD    = 0x%08X\n", ficd);
	}
	else{
		printf("   AFICD    = 0x%08X\n", ficd);
	}
	switch (ficd & MM_FICD_ICS_MASK){
		case MM_FICD_ICS_PAIR1:
			printf("                      %02x Use PGEC1/PGED1\n", MM_FICD_ICS_PAIR1);
			break;
		case MM_FICD_ICS_PAIR2:
			printf("                      %02x Use PGEC2/PGED2\n", MM_FICD_ICS_PAIR2);
			break;
		case MM_FICD_ICS_PAIR3:
			printf("                      %02x Use PGEC3/PGED3\n", MM_FICD_ICS_PAIR3);
			break;
		case MM_FICD_ICS_PAIRNONE:
			printf("                      %02x PGEC/PGED not connected\n", MM_FICD_ICS_PAIRNONE);
			break;
	}
	if (ficd & MM_FICD_JTAGEN){
		printf("                       %01x JTAG enabled\n", MM_FICD_JTAGEN);
	}
	else{
		printf("                       %01x JTAG disabled\n", 0);
	}
}

void print_mm_fpor(uint32_t fpor, uint32_t alternate){
	// FPOR register
	if (PRIMARY == alternate){
		printf("    FPOR    = 0x%08X\n", fpor);
	}
	else{
		printf("   AFPOR    = 0x%08X\n", fpor);
	}
	if (fpor & MM_FPOR_LPBOREN){
		printf("                       %01x Low power BOR enabled, when main BOR disabled\n", MM_FPOR_LPBOREN);
	}
	else{
		printf("                       %01x Low power BOR disabled\n", 0);
	}
	if (fpor & MM_FPOR_RETVR){
		printf("                       %01x Retention regulator disabled\n", MM_FPOR_RETVR);
	}
	else{
		printf("                       %01x Retention regulator enabled, RETEN in sleep\n", 0);
	}	
	switch (fpor & MM_FPOR_BOREN_MASK){
		case MM_FPOR_BOREN3:
			printf("                       %01x Brown-out Reset enabled in HW, SBOREN bit is disabled\n", MM_FPOR_BOREN3);
			break;
		case MM_FPOR_BOREN2:
			printf("                       %01x Brown-out Reset in enabled only while device is active and disabled in Sleep; SBOREN bit is disabled\n", MM_FPOR_BOREN2);
			break;
		case MM_FPOR_BOREN1:
			printf("                       %01x Brown-out Reset is controlled with the SBOREN bit setting\n", MM_FPOR_BOREN1);
			break;
		case MM_FPOR_BOREN0:
			printf("                       %01x Brown-out Reset is disabled in HWM SBOREN bit is disabled\n", MM_FPOR_BOREN0);
			break;
	}
}

void print_mm_fwdt(uint32_t fwdt, uint32_t alternate){
	// FWDT register
	if (PRIMARY == alternate){
		printf("    FWDT    = 0x%08X\n", fwdt);
	}
	else{
		printf("   AFWDT    = 0x%08X\n", fwdt);
	}
	if (fwdt & MM_FWDT_FWDTEN){
		printf("                    %01x    WDT is enabled\n", MM_FWDT_FWDTEN >> 12);
	}
	else{
		printf("                    %01x    WDT is disabled\n", 0);
	}

	switch (fwdt & MM_FWDT_RCLKSEL_MASK){
		case MM_FWDT_RCLKSEL_LPRC:
			printf("                    %01x    WDT clock source is LPRC, same as in sleep\n", MM_FWDT_RCLKSEL_LPRC >> 12);
			break;
		case MM_FWDT_RCLKSEL_FRC:
			printf("                    %01x    WDT clock source is FRC\n", MM_FWDT_RCLKSEL_FRC >> 12);
			break;
		case MM_FWDT_RCLKSEL_RES:
			printf("                    %01x    WDT clock source RESERVED!\n", MM_FWDT_RCLKSEL_RES >> 12);
			break;
		case MM_FWDT_RCLKSEL_SYS:
			printf("                    %01x    WDT clock source is system clock\n", MM_FWDT_RCLKSEL_SYS >> 12);
			break;
	}

	switch (fwdt & MM_FWDT_RWDTPS_MASK){
		case MM_FWDT_RWDTPS_1:
			printf("                    %02x   Run mode Watchdog postscale: 1/1\n", MM_FWDT_RWDTPS_1>>8);
			break;
		case MM_FWDT_RWDTPS_2:
			printf("                    %02x   Run mode Watchdog postscale: 1/2\n", MM_FWDT_RWDTPS_2>>8);
			break;
		case MM_FWDT_RWDTPS_4:
			printf("                    %02x   Run mode Watchdog postscale: 1/4\n", MM_FWDT_RWDTPS_4>>8);
			break;
		case MM_FWDT_RWDTPS_8:
			printf("                    %02x   Run mode Watchdog postscale: 1/8\n", MM_FWDT_RWDTPS_8>>8);
			break;
		case MM_FWDT_RWDTPS_16:
			printf("                    %02x   Run mode Watchdog postscale: 1/16\n", MM_FWDT_RWDTPS_16>>8);
			break;
		case MM_FWDT_RWDTPS_32:
			printf("                    %02x   Run mode Watchdog postscale: 1/32\n", MM_FWDT_RWDTPS_32>>8);
			break;
		case MM_FWDT_RWDTPS_64:
			printf("                    %02x   Run mode Watchdog postscale: 1/64\n", MM_FWDT_RWDTPS_64>>8);
			break;
		case MM_FWDT_RWDTPS_128:
			printf("                    %02x   Run mode Watchdog postscale: 1/128\n", MM_FWDT_RWDTPS_128>>8);
			break;
		case MM_FWDT_RWDTPS_256:
			printf("                    %02x   Run mode Watchdog postscale: 1/256\n", MM_FWDT_RWDTPS_256>>8);
			break;
		case MM_FWDT_RWDTPS_512:
			printf("                    %02x   Run mode Watchdog postscale: 1/512\n", MM_FWDT_RWDTPS_512>>8);
			break;
		case MM_FWDT_RWDTPS_1024:
			printf("                    %02x   Run mode Watchdog postscale: 1/1024\n", MM_FWDT_RWDTPS_1024>>8);
			break;
		case MM_FWDT_RWDTPS_2048:
			printf("                    %02x   Run mode Watchdog postscale: 1/2048\n", MM_FWDT_RWDTPS_2048>>8);
			break;
		case MM_FWDT_RWDTPS_4096:
			printf("                    %02x   Run mode Watchdog postscale: 1/4096\n", MM_FWDT_RWDTPS_4096>>8);
			break;
		case MM_FWDT_RWDTPS_8192:
			printf("                    %02x   Run mode Watchdog postscale: 1/8192\n", MM_FWDT_RWDTPS_8192>>8);
			break;
		case MM_FWDT_RWDTPS_16384:
			printf("                    %02x   Run mode Watchdog postscale: 1/16384\n", MM_FWDT_RWDTPS_16384>>8);
			break;
		case MM_FWDT_RWDTPS_32768:
			printf("                    %02x   Run mode Watchdog postscale: 1/32768\n", MM_FWDT_RWDTPS_32768>>8);
			break;
		case MM_FWDT_RWDTPS_65536:
			printf("                    %02x   Run mode Watchdog postscale: 1/65536\n", MM_FWDT_RWDTPS_65536>>8);
			break;
		case MM_FWDT_RWDTPS_131072:
			printf("                    %02x   Run mode Watchdog postscale: 1/131072\n", MM_FWDT_RWDTPS_131072>>8);
			break;
		case MM_FWDT_RWDTPS_262144:
			printf("                    %02x   Run mode Watchdog postscale: 1/262144\n", MM_FWDT_RWDTPS_262144>>8);
			break;
		case MM_FWDT_RWDTPS_524288:
			printf("                    %02x   Run mode Watchdog postscale: 1/524288\n", MM_FWDT_RWDTPS_524288>>8);
			break;
		default:
			printf("                    %02x   Run mode Watchdog postscale: 1/1048576\n", (fwdt & MM_FWDT_RWDTPS_MASK)>>8);
			break;
	}

	if (fwdt & MM_FWDT_WINDIS){
		printf("                      %01x  WDT Windowed mode disabled\n", MM_FWDT_WINDIS >> 4);
	}
	else{
		printf("                      %01x  WDT Windowed mode enbled\n", 0);
	}

	switch (fwdt & MM_FWDT_FWDTWINSZ_MASK){
		case MM_FWDT_FWDTWINSZ_25:
			printf("                      %01x  WDT window size is 25%%\n", MM_FWDT_FWDTWINSZ_25>>4);
			break;
		case MM_FWDT_FWDTWINSZ_375:
			printf("                      %01x  WDT window size is 37.5%%\n", MM_FWDT_FWDTWINSZ_375>>4);
			break;
		case MM_FWDT_FWDTWINSZ_50:
			printf("                      %01x  WDT window size is 50%%n", MM_FWDT_FWDTWINSZ_50>>4);
			break;
		case MM_FWDT_FWDTWINSZ_75:
			printf("                      %01x  WDT window size is 75%%\n", MM_FWDT_FWDTWINSZ_75>>4);
			break;
	}

	switch (fwdt & MM_FWDT_SWDTPS_MASK){
		case MM_FWDT_RWDTPS_1:
			printf("                      %02x Sleep mode Watchdog postscale: 1/1\n", MM_FWDT_RWDTPS_1);
			break;
		case MM_FWDT_SWDTPS_2:
			printf("                      %02x Sleep mode Watchdog postscale: 1/2\n", MM_FWDT_SWDTPS_2);
			break;
		case MM_FWDT_SWDTPS_4:
			printf("                      %02x Sleep mode Watchdog postscale: 1/4\n", MM_FWDT_SWDTPS_4);
			break;
		case MM_FWDT_SWDTPS_8:
			printf("                      %02x Sleep mode Watchdog postscale: 1/8\n", MM_FWDT_SWDTPS_8);
			break;
		case MM_FWDT_SWDTPS_16:
			printf("                      %02x Sleep mode Watchdog postscale: 1/16\n", MM_FWDT_SWDTPS_16);
			break;
		case MM_FWDT_SWDTPS_32:
			printf("                      %02x Sleep mode Watchdog postscale: 1/32\n", MM_FWDT_SWDTPS_32);
			break;
		case MM_FWDT_SWDTPS_64:
			printf("                      %02x Sleep mode Watchdog postscale: 1/64\n", MM_FWDT_SWDTPS_64);
			break;
		case MM_FWDT_SWDTPS_128:
			printf("                      %02x Sleep mode Watchdog postscale: 1/128\n", MM_FWDT_SWDTPS_128);
			break;
		case MM_FWDT_SWDTPS_256:
			printf("                      %02x Sleep mode Watchdog postscale: 1/256\n", MM_FWDT_SWDTPS_256);
			break;
		case MM_FWDT_SWDTPS_512:
			printf("                      %02x Sleep mode Watchdog postscale: 1/512\n", MM_FWDT_SWDTPS_512);
			break;
		case MM_FWDT_SWDTPS_1024:
			printf("                      %02x Sleep mode Watchdog postscale: 1/1024\n", MM_FWDT_SWDTPS_1024);
			break;
		case MM_FWDT_SWDTPS_2048:
			printf("                      %02x Sleep mode Watchdog postscale: 1/2048\n", MM_FWDT_SWDTPS_2048);
			break;
		case MM_FWDT_SWDTPS_4096:
			printf("                      %02x Sleep mode Watchdog postscale: 1/4096\n", MM_FWDT_SWDTPS_4096);
			break;
		case MM_FWDT_SWDTPS_8192:
			printf("                      %02x Sleep mode Watchdog postscale: 1/8192\n", MM_FWDT_SWDTPS_8192);
			break;
		case MM_FWDT_SWDTPS_16384:
			printf("                      %02x Sleep mode Watchdog postscale: 1/16384\n", MM_FWDT_SWDTPS_16384);
			break;
		case MM_FWDT_SWDTPS_32768:
			printf("                      %02x Sleep mode Watchdog postscale: 1/32768\n", MM_FWDT_SWDTPS_32768);
			break;
		case MM_FWDT_SWDTPS_65536:
			printf("                      %02x Sleep mode Watchdog postscale: 1/65536\n", MM_FWDT_SWDTPS_65536);
			break;
		case MM_FWDT_SWDTPS_131072:
			printf("                      %02x Sleep mode Watchdog postscale: 1/131072\n", MM_FWDT_SWDTPS_131072);
			break;
		case MM_FWDT_SWDTPS_262144:
			printf("                      %02x Sleep mode Watchdog postscale: 1/262144\n", MM_FWDT_SWDTPS_262144);
			break;
		case MM_FWDT_SWDTPS_524288:
			printf("                      %02x Sleep mode Watchdog postscale: 1/524288\n", MM_FWDT_SWDTPS_524288);
			break;
		default:
			printf("                      %02x Sleep mode Watchdog postscale: 1/1048576\n", (fwdt & MM_FWDT_SWDTPS_MASK));
			break;
	}
}

void print_mm_foscsel(uint32_t foscsel, uint32_t alternate){
	// FOSCSEL register
	if (PRIMARY == alternate){
		printf("    FOSCSEL = 0x%08X\n", foscsel);
	}
	else{
		printf("   AFOSCSEL = 0x%08X\n", foscsel);
	}

	switch (foscsel & MM_FOSCSEL_FCKSM_MASK){
		case MM_FOSCSEL_FCKSM3:
			printf("                    %01x    Clock switching enabled, Fail safe monitor enabled\n", MM_FOSCSEL_FCKSM3 >> 12);
			break;
		case MM_FOSCSEL_FCKSM2:
			printf("                    %01x    Clock switching disabled, Fail safe monitor enabled\n", MM_FOSCSEL_FCKSM2 >> 12);
			break;
		case MM_FOSCSEL_FCKSM1:
			printf("                    %01x    Clock switching enabled, Fail safe monitor disabled\n", MM_FOSCSEL_FCKSM1 >> 12);
			break;
		case MM_FOSCSEL_FCKSM0:
			printf("                    %01x    Clock switching disabled, Fail safe monitor disabled\n", MM_FOSCSEL_FCKSM0 >> 12);
			break;
	}

	if (foscsel & MM_FOSCSEL_SOSCSEL){
		printf("                    %01x    SOSC crystal used (pins controlled by SOSC) \n", MM_FOSCSEL_SOSCSEL >> 12);
	}
	else{
		printf("                    %01x    External clock connected to SOSCO, pins controlled by PORTx\n", 0);
	}

	if (foscsel & MM_FOSCSEL_OSCIOFNC){
		printf("                     %01x   OSC2/CLKO pin operated as normal I/O\n", MM_FOSCSEL_OSCIOFNC >> 8);
	}
	else{
		printf("                     %01x   System clock connected to pin OSC2/CLKO\n", 0);
	}

	switch (foscsel & MM_FOSCSEL_POSCMOD_MASK){
		case MM_FOSCSEL_POSCMOD_DIS:
			printf("                     %01x   Primary oscillator disabled\n", MM_FOSCSEL_POSCMOD_DIS >> 8);
			break;
		case MM_FOSCSEL_POSCMOD_HS:
			printf("                     %01x   HS Oscillator selected\n", MM_FOSCSEL_POSCMOD_HS >> 8);
			break;
		case MM_FOSCSEL_POSCMOD_XT:
			printf("                     %01x   XT Oscillator selected\n", MM_FOSCSEL_POSCMOD_XT >> 8);
			break;
		case MM_FOSCSEL_POSCMOD_EC:
			printf("                     %01x   EC (External Clock) selected\n", MM_FOSCSEL_POSCMOD_EC >> 8);
			break;
	}

	if (foscsel & MM_FOSCSEL_IESO){
		printf("                      %01x  Two-speed startup enabled\n", MM_FOSCSEL_IESO >> 4);
	}
	else{
		printf("                      %01x  Two-speed startup disabled\n", 0);
	}

	if (foscsel & MM_FOSCSEL_SOSCEN){
		printf("                      %01x  Secondary oscillator enabled\n", MM_FOSCSEL_SOSCEN >> 4);
	}
	else{
		printf("                      %01x  Secondary oscillator disabled\n", 0);
	}

	if (foscsel & MM_FOSCSEL_PLLSRC){
		printf("                      %01x  FRC is input to PLL on reset\n", MM_FOSCSEL_PLLSRC >> 4);
	}
	else{
		printf("                      %01x  Primary oscillator (POSC) is input to PLL on reset\n", 0);
	}

	switch (foscsel & MM_FOSCSEL_FNOSC_MASK){
		case MM_FOSCSEL_FNOSC_PRIM_FRC_PLL:
			printf("                       %01x Primary or FRC oscillator + PLL\n", MM_FOSCSEL_FNOSC_PRIM_FRC_PLL);
			break;
		case MM_FOSCSEL_FNOSC_PRIM:
			printf("                       %01x Primary oscillator (XT, HS, EC)\n", MM_FOSCSEL_FNOSC_PRIM);
			break;
		case MM_FOSCSEL_FNOSC_RESERVED:
			printf("                       %01x Reserved - check your settings!\n", MM_FOSCSEL_FNOSC_RESERVED);
			break;
		case MM_FOSCSEL_FNOSC_SOCS:
			printf("                       %01x Secondary oscillator (SOSC)\n", MM_FOSCSEL_FNOSC_SOCS);
			break;
		case MM_FOSCSEL_FNOSC_LPRC:
			printf("                       %01x Low-power RC oscillator (LPRC)\n", MM_FOSCSEL_FNOSC_LPRC);
			break;
		default:
			printf("                       %01x Fast RC (FRC) with Divide-by-N\n", (foscsel & MM_FOSCSEL_FNOSC_MASK));
			break;
	}

}

void print_mm_fsec(uint32_t fsec, uint32_t alternate){
	// FSEC register
	if (PRIMARY == alternate){
		printf("    FSEC = 0x%08X\n", fsec);
	}
	else{
		printf("   AFSEC = 0x%08X\n", fsec);
	}
	if (fsec & MM_FSEC_CP){
		printf("             %01x           Code protection disabled\n", MM_FSEC_CP>>28);
	}
	else{
		printf("             %01x           Code protection enabled\n", 0);
	}

}
