/*
 * Routines specific for PIC32 MK family.
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

void print_mk_devcfg3(uint32_t devcfg3, uint32_t alternate);
void print_mk_devcfg2(uint32_t devcfg2, uint32_t alternate);
void print_mk_devcfg1(uint32_t devcfg1, uint32_t alternate);
void print_mk_devcfg0(uint32_t devcfg0, uint32_t alternate);
void print_mk_devcp(uint32_t devcp, uint32_t alternate);
void print_mk_devsign(uint32_t devsign, uint32_t alternate);
void print_mk_devseq(uint32_t devseq, uint32_t alternate);
void print_mk_devsn(uint32_t devsn0, uint32_t devsn1, uint32_t devsn2,
					uint32_t devsn3);

/*
 * Print configuration for MK family.
 */
void print_mk(unsigned cfg0, unsigned cfg1, unsigned cfg2, unsigned cfg3,
				unsigned cfg4, unsigned cfg5, unsigned cfg6, unsigned cfg7,
				unsigned cfg8, unsigned cfg9, unsigned cfg10, unsigned cfg11,
				unsigned cfg12, unsigned cfg13, unsigned cfg14, unsigned cfg15,
				unsigned cfg16, unsigned cfg17)
{
	printf("Boot flash 1 bits\n");
	print_mk_devcfg3(cfg0, PRIMARY);
	print_mk_devcfg2(cfg1, PRIMARY);
	print_mk_devcfg1(cfg2, PRIMARY);
	print_mk_devcfg0(cfg3, PRIMARY);
	print_mk_devcp(cfg4, PRIMARY);
	print_mk_devsign(cfg5, PRIMARY);
	print_mk_devseq(cfg6, PRIMARY);

	printf("Boot flash 2 bits\n");
	print_mk_devcfg3(cfg7, ALTERNATE);
	print_mk_devcfg2(cfg8, ALTERNATE);
	print_mk_devcfg1(cfg9, ALTERNATE);
	print_mk_devcfg0(cfg10, ALTERNATE);
	print_mk_devcp(cfg11, ALTERNATE);
	print_mk_devsign(cfg12, ALTERNATE);
	print_mk_devseq(cfg13, ALTERNATE);

	// UUID of chip
	print_mk_devsn(cfg14, cfg15, cfg16, cfg17);

	// Write out which is in the Lower Boot Alias, and which Upper
	// If DEVSEQ TSEQ val in Boot flash 1 is >= than in Boot flash 2,
	// Then it is the Lower Boot Alias. Else it is Boot flash 2.
	if ((cfg6 & 0xFFFF) >= (cfg13 & 0xFFFF)){
		printf(" Boot flash 1 aliased by Lower Boot Alias.\n Boot flash 2 is aliased by Upper Boot alias\n");
	}
	else{
		printf(" Boot flash 2 aliased by Lower Boot Alias.\n Boot flash 1 is aliased by Upper Boot Alias\n");
	}

}

// When printing values, shift so the value fits into the first 4 bits.
// That way, can align it under each nibble.
// If there's more bits, shift so it fits into 8 bits, and display as %02x

// ALL values are in hex.

void print_mk_devsn(uint32_t devsn0, uint32_t devsn1, uint32_t devsn2,
					uint32_t devsn3){
	printf(" UUID: 0x%08x 0x%08x 0x%08x 0x%08x\n", devsn0, devsn1, devsn2, devsn3);
}

void print_mk_devcfg3(uint32_t devcfg3, uint32_t alternate){
	// DEVCFG3
	if (PRIMARY == alternate){
		printf(" BF1DEVCFG3 = 0x%08X\n", devcfg3);
	}
	else{
		printf(" BF2DEVCFG3 = 0x%08X\n", devcfg3);
	}
	
	if (devcfg3 & MK_DEVCFG3_FVBUSIO1){
		printf("                %01x        VBUSON pin: controlled by USB1 module\n", MK_DEVCFG3_FVBUSIO1 >> 28);
	}
	else{
		printf("                %01x        VBUSON pin: controlled by port function\n", 0);
	}
	
	if (devcfg3 & MK_DEVCFG3_FUSBIDIO1){
		printf("                %01x        USBID pin: controlled by USB1 module\n", MK_DEVCFG3_FUSBIDIO1 >> 28);
	}
	else{
		printf("                %01x        USBID pin: controlled by port function\n", 0);
	}

	if (devcfg3 & MK_DEVCFG3_IOL1WAY){
		printf("                %01x        Peripheral Pin Select - Allow only one configuration\n", MK_DEVCFG3_IOL1WAY >> 28);
	}
	else{
		printf("                %01x        Peripheral Pin Select - Allow multiple configurations\n", 0);
	}

	if (devcfg3 & MK_DEVCFG3_PMDL1WAY){
		printf("                %01x        Permission Module Disable - Allow only one configuration\n", MK_DEVCFG3_PMDL1WAY >> 28);
	}
	else{
		printf("                %01x        Permission Module Disable - Allow multiple configurations\n", 0);
	}

	if (devcfg3 & MK_DEVCFG3_PGL1WAY){
		printf("                 %01x       Permission Group Lock - Allow only one configuration\n", MK_DEVCFG3_PGL1WAY >> 24);
	}
	else{
		printf("                 %01x       Permission Group Lock - Allow multiple configurations\n", 0);
	}

	if (devcfg3 & MK_DEVCFG3_FVBUSIO2){
		printf("                  %01x      VBUSON pin: controlled by USB2 module\n", MK_DEVCFG3_FVBUSIO2 >> 20);
	}
	else{
		printf("                  %01x      VBUSON pin: controlled by port function\n", 0);
	}
	
	if (devcfg3 & MK_DEVCFG3_FUSBIDIO2){
		printf("                  %01x      USBID pin: controlled by USB2 module\n", MK_DEVCFG3_FUSBIDIO2 >> 20);
	}
	else{
		printf("                  %01x      USBID pin: controlled by port function\n", 0);
	}

	if (devcfg3 & MK_DEVCFG3_PWMLOCK){
		printf("                  %01x      Write access to PWM IOCONx not locked\n", MK_DEVCFG3_PWMLOCK >> 20);
	}
	else{
		printf("                  %01x      Write access to PWM IOCONx locked\n", 0);
	}

	printf("                    %04X USERID\n", devcfg3 & MK_DEVCFG3_USERID_MASK);

}

void print_mk_devcfg2(uint32_t devcfg2, uint32_t alternate){
	// DEVCFG2
	if (PRIMARY == alternate){
		printf(" BF1DEVCFG2 = 0x%08X\n", devcfg2);
	}
	else{
		printf(" BF2DEVCFG2 = 0x%08X\n", devcfg2);
	}

	if (devcfg2 & MK_DEVCFG2_UPLLEN){
		printf("                %01x        USB PLL is disabled\n", MK_DEVCFG2_UPLLEN >> 28);
	}
	else{
		printf("                %01x        USB PLL is disabled\n", 0);
	}

	if (devcfg2 & MK_DEVCFG2_BORSEL){
		printf("                %01x        BOR trip voltage 2.1V (non-Opamp device operation)\n", MK_DEVCFG2_BORSEL >> 28);
	}
	else{
		printf("                %01x        BOR trip voltage 2.8V (Opamp device operation)\n", 0);
	}

	if (devcfg2 & MK_DEVCFG2_FDSEN){
		printf("                %01x        DS bit (DSCON<15>) is enabled on WAIT command\n", MK_DEVCFG2_FDSEN >> 28);
	}
	else{
		printf("                %01x        DS bit (DSCON<15>) is disabled\n", 0);
	}

	if (devcfg2 & MK_DEVCFG2_DSWDTEN){
		printf("                 %01x       Enable DSWDT dring Deep Sleep\n", MK_DEVCFG2_DSWDTEN >> 24);
	}
	else{
		printf("                 %01x       Disable DSWDT dring Deep Sleep\n", 0);
	}

	if (devcfg2 & MK_DEVCFG2_DSWDTOSC){
		printf("                 %01x       LPRC as DSWDT reference clock\n", MK_DEVCFG2_DSWDTOSC >> 24);
	}
	else{
		printf("                 %01x       SOSC as DSWDT reference clock\n", 0);
	}

	switch (devcfg2 & MK_DEVCFG2_DSWDTPS_MASK){
		case MK_DEVCFG2_DSWDTPS_236:
			printf("                 %02x      WDT Postscale 1:2^36 (25.7 days)\n", (devcfg2 & MK_DEVCFG2_DSWDTPS_MASK) >> 20);
			break;
		case MK_DEVCFG2_DSWDTPS_235:
			printf("                 %02x      WDT Postscale 1:2^35 (12.8 days)\n", (devcfg2 & MK_DEVCFG2_DSWDTPS_MASK) >> 20);
			break;
		case MK_DEVCFG2_DSWDTPS_234:
			printf("                 %02x      WDT Postscale 1:2^34 (6.4 days)\n", (devcfg2 & MK_DEVCFG2_DSWDTPS_MASK) >> 20);
			break;
		case MK_DEVCFG2_DSWDTPS_233:
			printf("                 %02x      WDT Postscale 1:2^33 (77.0 hours)\n", (devcfg2 & MK_DEVCFG2_DSWDTPS_MASK) >> 20);
			break;
		case MK_DEVCFG2_DSWDTPS_232:
			printf("                 %02x      WDT Postscale 1:2^32 (38.5 hours)\n", (devcfg2 & MK_DEVCFG2_DSWDTPS_MASK) >> 20);
			break;
		case MK_DEVCFG2_DSWDTPS_231:
			printf("                 %02x      WDT Postscale 1:2^31 (19.2 hours)\n", (devcfg2 & MK_DEVCFG2_DSWDTPS_MASK) >> 20);
			break;
		case MK_DEVCFG2_DSWDTPS_230:
			printf("                 %02x      WDT Postscale 1:2^30 (9.6 hours)\n", (devcfg2 & MK_DEVCFG2_DSWDTPS_MASK) >> 20);
			break;
		case MK_DEVCFG2_DSWDTPS_229:
			printf("                 %02x      WDT Postscale 1:2^29 (4.8 hours)\n", (devcfg2 & MK_DEVCFG2_DSWDTPS_MASK) >> 20);
			break;
		case MK_DEVCFG2_DSWDTPS_228:
			printf("                 %02x      WDT Postscale 1:2^28 (2.4 hours)\n", (devcfg2 & MK_DEVCFG2_DSWDTPS_MASK) >> 20);
			break;
		case MK_DEVCFG2_DSWDTPS_227:
			printf("                 %02x      WDT Postscale 1:2^27 (72.2 minutes)\n", (devcfg2 & MK_DEVCFG2_DSWDTPS_MASK) >> 20);
			break;
		case MK_DEVCFG2_DSWDTPS_226:
			printf("                 %02x      WDT Postscale 1:2^26 (36.1 minutes)\n", (devcfg2 & MK_DEVCFG2_DSWDTPS_MASK) >> 20);
			break;
		case MK_DEVCFG2_DSWDTPS_225:
			printf("                 %02x      WDT Postscale 1:2^25 (18.0 minutes)\n", (devcfg2 & MK_DEVCFG2_DSWDTPS_MASK) >> 20);
			break;
		case MK_DEVCFG2_DSWDTPS_224:
			printf("                 %02x      WDT Postscale 1:2^24 (9.0 minutes)\n", (devcfg2 & MK_DEVCFG2_DSWDTPS_MASK) >> 20);
			break;
		case MK_DEVCFG2_DSWDTPS_223:
			printf("                 %02x      WDT Postscale 1:2^23 (4.5 minutes)\n", (devcfg2 & MK_DEVCFG2_DSWDTPS_MASK) >> 20);
			break;
		case MK_DEVCFG2_DSWDTPS_222:
			printf("                 %02x      WDT Postscale 1:2^22 (135.5 seconds)\n", (devcfg2 & MK_DEVCFG2_DSWDTPS_MASK) >> 20);
			break;
		case MK_DEVCFG2_DSWDTPS_221:
			printf("                 %02x      WDT Postscale 1:2^21 (67.7 seconds)\n", (devcfg2 & MK_DEVCFG2_DSWDTPS_MASK) >> 20);
			break;
		case MK_DEVCFG2_DSWDTPS_220:
			printf("                 %02x      WDT Postscale 1:2^20 (33.825 seconds)\n", (devcfg2 & MK_DEVCFG2_DSWDTPS_MASK) >> 20);
			break;
		case MK_DEVCFG2_DSWDTPS_219:
			printf("                 %02x      WDT Postscale 1:2^19 (16.912 seconds)\n", (devcfg2 & MK_DEVCFG2_DSWDTPS_MASK) >> 20);
			break;
		case MK_DEVCFG2_DSWDTPS_218:
			printf("                 %02x      WDT Postscale 1:2^18 (8.456 seconds)\n", (devcfg2 & MK_DEVCFG2_DSWDTPS_MASK) >> 20);
			break;
		case MK_DEVCFG2_DSWDTPS_217:
			printf("                 %02x      WDT Postscale 1:2^17 (4.228 seconds)\n", (devcfg2 & MK_DEVCFG2_DSWDTPS_MASK) >> 20);
			break;
		case MK_DEVCFG2_DSWDTPS_65536:
			printf("                 %02x      WDT Postscale 1:2^16 (2.114 seconds)\n", (devcfg2 & MK_DEVCFG2_DSWDTPS_MASK) >> 20);
			break;
		case MK_DEVCFG2_DSWDTPS_32768:
			printf("                 %02x      WDT Postscale 1:2^15 (1.057 seconds)\n", (devcfg2 & MK_DEVCFG2_DSWDTPS_MASK) >> 20);
			break;
		case MK_DEVCFG2_DSWDTPS_16384:
			printf("                 %02x      WDT Postscale 1:2^14 (528.5 milliseconds)\n", (devcfg2 & MK_DEVCFG2_DSWDTPS_MASK) >> 20);
			break;
		case MK_DEVCFG2_DSWDTPS_8192:
			printf("                 %02x      WDT Postscale 1:2^13 (264.3 milliseconds)\n", (devcfg2 & MK_DEVCFG2_DSWDTPS_MASK) >> 20);
			break;
		case MK_DEVCFG2_DSWDTPS_4096:
			printf("                 %02x      WDT Postscale 1:2^12 (132.1 milliseconds)\n", (devcfg2 & MK_DEVCFG2_DSWDTPS_MASK) >> 20);
			break;
		case MK_DEVCFG2_DSWDTPS_2048:
			printf("                 %02x      WDT Postscale 1:2^11 (66.1 milliseconds)\n", (devcfg2 & MK_DEVCFG2_DSWDTPS_MASK) >> 20);
			break;
		case MK_DEVCFG2_DSWDTPS_1024:
			printf("                 %02x      WDT Postscale 1:2^10 (33 milliseconds)\n", (devcfg2 & MK_DEVCFG2_DSWDTPS_MASK) >> 20);
			break;
		case MK_DEVCFG2_DSWDTPS_512:
			printf("                 %02x      WDT Postscale 1:2^9 (16.5 milliseconds)\n", (devcfg2 & MK_DEVCFG2_DSWDTPS_MASK) >> 20);
			break;
		case MK_DEVCFG2_DSWDTPS_256:
			printf("                 %02x      WDT Postscale 1:2^8 (8.3 milliseconds)\n", (devcfg2 & MK_DEVCFG2_DSWDTPS_MASK) >> 20);
			break;
		case MK_DEVCFG2_DSWDTPS_128:
			printf("                 %02x      WDT Postscale 1:2^7 (4.1 milliseconds)\n", (devcfg2 & MK_DEVCFG2_DSWDTPS_MASK) >> 20);
			break;
		case MK_DEVCFG2_DSWDTPS_64:
			printf("                 %02x      WDT Postscale 1:2^6 (2.1 milliseconds)\n", (devcfg2 & MK_DEVCFG2_DSWDTPS_MASK) >> 20);
			break;
		case MK_DEVCFG2_DSWDTPS_32:
			printf("                 %02x      WDT Postscale 1:2^5 (1 millisecond)\n", (devcfg2 & MK_DEVCFG2_DSWDTPS_MASK) >> 20);
			break;
	}

	if (devcfg2 & MK_DEVCFG2_DSBOREN){
		printf("                  %01x      Enable ZPBOR during deep sleep\n", MK_DEVCFG2_DSBOREN >> 20);
	}
	else{
		printf("                  %01x      Disable ZPBOR during deep sleep\n", 0);
	}

	if (devcfg2 & MK_DEVCFG2_VBATBOREN){
		printf("                   %01x     Enable ZPBOR during VBAT mode\n", MK_DEVCFG2_VBATBOREN >> 16);
	}
	else{
		printf("                   %01x     Disable ZPBOR during VBAT mode\n", 0);
	}

	switch (devcfg2 & MK_DEVCFG2_FPLLODIV_MASK){
		case MK_DEVCFG2_FPLLODIV_32_1:
		case MK_DEVCFG2_FPLLODIV_32_2:
		case MK_DEVCFG2_FPLLODIV_32_3:
			printf("                   %01x     PLL output divided by 32\n", (devcfg2 & MK_DEVCFG2_FPLLODIV_MASK) >> 16);
			break;
		case MK_DEVCFG2_FPLLODIV_16:
			printf("                   %01x     PLL output divided by 16\n", (devcfg2 & MK_DEVCFG2_FPLLODIV_MASK) >> 16);
			break;
		case MK_DEVCFG2_FPLLODIV_8:
			printf("                   %01x     PLL output divided by 8\n", (devcfg2 & MK_DEVCFG2_FPLLODIV_MASK) >> 16);
			break;
		case MK_DEVCFG2_FPLLODIV_4:
			printf("                   %01x     PLL output divided by 4\n", (devcfg2 & MK_DEVCFG2_FPLLODIV_MASK) >> 16);
			break;
		case MK_DEVCFG2_FPLLODIV_2_1:
		case MK_DEVCFG2_FPLLODIV_2_2:
			printf("                   %01x     PLL output divided by 2\n", (devcfg2 & MK_DEVCFG2_FPLLODIV_MASK) >> 16);
			break;
	}

	printf("                    %02x   PLL output divided by %d\n", 
			(devcfg2 & MK_DEVCFG2_FPLLMULT_MASK) >> MK_DEVCFG2_FPLLMULT_SHIFT,
			((devcfg2 & MK_DEVCFG2_FPLLMULT_MASK) >> MK_DEVCFG2_FPLLMULT_SHIFT) + MK_DEVCFG2_FPLLMULT_MIN_VAL);

	if (devcfg2 & MK_DEVCFG2_FPLLICLK){
		printf("                      %01x  FRC is selected as input to System PLL\n", MK_DEVCFG2_FPLLICLK >> 4);
	}
	else{
		printf("                      %01x  POSC is selected as input to System PLL\n", 0);
	}

	switch (devcfg2 & MK_DEVCFG2_FPLLRNG_MASK){
		case MK_DEVCFG2_FPLLRNG_34_64:
			printf("                      %01x  System PLL Input clock range 34-64 MHz\n", (devcfg2 & MK_DEVCFG2_FPLLRNG_MASK) >> 4);
			break;
		case MK_DEVCFG2_FPLLRNG_21_42:
			printf("                      %01x  System PLL Input clock range 21-42 MHz\n", (devcfg2 & MK_DEVCFG2_FPLLRNG_MASK) >> 4);
			break;
		case MK_DEVCFG2_FPLLRNG_13_26:
			printf("                      %01x  System PLL Input clock range 13-26 MHz\n", (devcfg2 & MK_DEVCFG2_FPLLRNG_MASK) >> 4);
			break;
		case MK_DEVCFG2_FPLLRNG_8_16:
			printf("                      %01x  System PLL Input clock range 8-16 MHz\n", (devcfg2 & MK_DEVCFG2_FPLLRNG_MASK) >> 4);
			break;
		case MK_DEVCFG2_FPLLRNG_5_10:
			printf("                      %01x  System PLL Input clock range 5-10 MHz\n", (devcfg2 & MK_DEVCFG2_FPLLRNG_MASK) >> 4);
			break;
		case MK_DEVCFG2_FPLLRNG_BYPASS:
			printf("                      %01x  System PLL Input clock range BYPASS\n", (devcfg2 & MK_DEVCFG2_FPLLRNG_MASK) >> 4);
			break;
		default:
			printf("                      %01x  System PLL Input clock range RESERVED\n", (devcfg2 & MK_DEVCFG2_FPLLRNG_MASK) >> 4);
			break;

	}

	switch (devcfg2 & MK_DEVCFG2_FPLLIDIV_MASK){
		case MK_DEVCFG2_FPLLIDIV_8:
			printf("                       %01x PLL input - Divide by 8\n", devcfg2 & MK_DEVCFG2_FPLLIDIV_MASK);
			break;
		case MK_DEVCFG2_FPLLIDIV_7:
			printf("                       %01x PLL input - Divide by 7\n", devcfg2 & MK_DEVCFG2_FPLLIDIV_MASK);
			break;
		case MK_DEVCFG2_FPLLIDIV_6:
			printf("                       %01x PLL input - Divide by 6\n", devcfg2 & MK_DEVCFG2_FPLLIDIV_MASK);
			break;
		case MK_DEVCFG2_FPLLIDIV_5:
			printf("                       %01x PLL input - Divide by 5\n", devcfg2 & MK_DEVCFG2_FPLLIDIV_MASK);
			break;
		case MK_DEVCFG2_FPLLIDIV_4:
			printf("                       %01x PLL input - Divide by 4\n", devcfg2 & MK_DEVCFG2_FPLLIDIV_MASK);
			break;
		case MK_DEVCFG2_FPLLIDIV_3:
			printf("                       %01x PLL input - Divide by 3\n", devcfg2 & MK_DEVCFG2_FPLLIDIV_MASK);
			break;
		case MK_DEVCFG2_FPLLIDIV_2:
			printf("                       %01x PLL input - Divide by 2\n", devcfg2 & MK_DEVCFG2_FPLLIDIV_MASK);
			break;
		case MK_DEVCFG2_FPLLIDIV_1:
			printf("                       %01x PLL input - Divide by 1\n", devcfg2 & MK_DEVCFG2_FPLLIDIV_MASK);
			break;
	}

}


void print_mk_devcfg1(uint32_t devcfg1, uint32_t alternate){
	// DEVCFG1
	if (PRIMARY == alternate){
		printf(" BF1DEVCFG1 = 0x%08X\n", devcfg1);
	}
	else{
		printf(" BF2DEVCFG1 = 0x%08X\n", devcfg1);
	}

	if (devcfg1 & MK_DEVCFG1_FDMTEN){
		printf("                %01x        Deadman Timer enabled and CANNOT be disabled in SW\n", MK_DEVCFG1_FDMTEN >> 28);
	}
	else{
		printf("                %01x        Deadman Timer disabled and can be enabled in SW\n", 0);
	}

	if ( 	((devcfg1 & MK_DEVCFG1_DMTCNT_MASK) >> MK_DEVCFG1_DMTCNT_SHIFT) > 
			(MK_DEVCFG1_DMTCNT_MAX_EXPONENT - MK_DEVCFG1_DMTCNT_MIN_EXPONENT)){
		printf("                %02x       Deadman Timer Count Select: RESERVED\n", (devcfg1 & MK_DEVCFG1_DMTCNT_MASK) >> 24);
	}
	else{
		printf("                %02x       Deadman Timer Count Select: 2^%d (%d)\n", 
								(devcfg1 & MK_DEVCFG1_DMTCNT_MASK) >> 24, 
								8 + ((devcfg1 & MK_DEVCFG1_DMTCNT_MASK) >> MK_DEVCFG1_DMTCNT_SHIFT),
								1<<(8 + ((devcfg1 & MK_DEVCFG1_DMTCNT_MASK) >> MK_DEVCFG1_DMTCNT_SHIFT)));
	}

	switch (devcfg1 & MK_DEVCFG1_FWDTWINSZ_MASK){
		case MK_DEVCFG1_FWDTWINSZ_25:
			printf("                 %01x       Watchdog Timer Window Size 25%%\n", (devcfg1 & MK_DEVCFG1_FWDTWINSZ_MASK) >> 24);
			break;
		case MK_DEVCFG1_FWDTWINSZ_27_5:
			printf("                 %01x       Watchdog Timer Window Size 37.5%%\n", (devcfg1 & MK_DEVCFG1_FWDTWINSZ_MASK) >> 24);
			break;
		case MK_DEVCFG1_FWDTWINSZ_50:
			printf("                 %01x       Watchdog Timer Window Size 50%%\n", (devcfg1 & MK_DEVCFG1_FWDTWINSZ_MASK) >> 24);
			break;
		case MK_DEVCFG1_FWDTWINSZ_75:
			printf("                 %01x       Watchdog Timer Window Size 75%%\n", (devcfg1 & MK_DEVCFG1_FWDTWINSZ_MASK) >> 24);
			break;
	}

	if (devcfg1 & MK_DEVCFG1_FWDTEN){
		printf("                  %01x      Watchdog timer is enabled, CANNOT be disabled in SW\n", MK_DEVCFG1_FWDTEN >> 20);
	}
	else{
		printf("                  %01x      Watchdog Timer disabled and can be enabled in SW\n", 0);
	}

	if (devcfg1 & MK_DEVCFG1_WINDIS){
		printf("                  %01x      Watchdog timer is in non-window mode\n", MK_DEVCFG1_WINDIS >> 20);
	}
	else{
		printf("                  %01x      Watchdog Timer is in window mode\n", 0);
	}

	if (devcfg1 & MK_DEVCFG1_WDTSPGM){
		printf("                  %01x      Watchdog timer stops during Flash programming\n", MK_DEVCFG1_WDTSPGM >> 20);
	}
	else{
		printf("                  %01x      Watchdog Timer runs during Flash programming\n", 0);
	}


	switch (devcfg1 & MK_DEVCFG1_WDTPS_MASK){
		case MK_DEVCFG1_WDTPS_524288:
			printf("                  %02x     Watchdog Timer Postscale 1:524288\n", MK_DEVCFG1_WDTPS_1048576 >> 16);
			break;
		case MK_DEVCFG1_WDTPS_262144:
			printf("                  %02x     Watchdog Timer Postscale 1:262144\n", MK_DEVCFG1_WDTPS_262144 >> 16);
			break;
		case MK_DEVCFG1_WDTPS_131072:
			printf("                  %02x     Watchdog Timer Postscale 1:131072\n", MK_DEVCFG1_WDTPS_131072 >> 16);
			break;
		case MK_DEVCFG1_WDTPS_65536:
			printf("                  %02x     Watchdog Timer Postscale 1:65536\n", MK_DEVCFG1_WDTPS_65536 >> 16);
			break;
		case MK_DEVCFG1_WDTPS_32768:
			printf("                  %02x     Watchdog Timer Postscale 1:32768\n", MK_DEVCFG1_WDTPS_32768 >> 16);
			break;
		case MK_DEVCFG1_WDTPS_16384:
			printf("                  %02x     Watchdog Timer Postscale 1:16384\n", MK_DEVCFG1_WDTPS_16384 >> 16);
			break;
		case MK_DEVCFG1_WDTPS_8192:
			printf("                  %02x     Watchdog Timer Postscale 1:8192\n", MK_DEVCFG1_WDTPS_8192 >> 16);
			break;
		case MK_DEVCFG1_WDTPS_4096:
			printf("                  %02x     Watchdog Timer Postscale 1:4096\n", MK_DEVCFG1_WDTPS_4096 >> 16);
			break;
		case MK_DEVCFG1_WDTPS_2048:
			printf("                  %02x     Watchdog Timer Postscale 1:2048\n", MK_DEVCFG1_WDTPS_2048 >> 16);
			break;
		case MK_DEVCFG1_WDTPS_1024:
			printf("                  %02x     Watchdog Timer Postscale 1:1024\n", MK_DEVCFG1_WDTPS_1024 >> 16);
			break;
		case MK_DEVCFG1_WDTPS_512:
			printf("                  %02x     Watchdog Timer Postscale 1:512\n", MK_DEVCFG1_WDTPS_512 >> 16);
			break;
		case MK_DEVCFG1_WDTPS_256:
			printf("                  %02x     Watchdog Timer Postscale 1:256\n", MK_DEVCFG1_WDTPS_256 >> 16);
			break;
		case MK_DEVCFG1_WDTPS_128:
			printf("                  %02x     Watchdog Timer Postscale 1:128\n", MK_DEVCFG1_WDTPS_128 >> 16);
			break;
		case MK_DEVCFG1_WDTPS_64:
			printf("                  %02x     Watchdog Timer Postscale 1:64\n", MK_DEVCFG1_WDTPS_64 >> 16);
			break;
		case MK_DEVCFG1_WDTPS_32:
			printf("                  %02x     Watchdog Timer Postscale 1:32\n", MK_DEVCFG1_WDTPS_32 >> 16);
			break;
		case MK_DEVCFG1_WDTPS_16:
			printf("                  %02x     Watchdog Timer Postscale 1:16\n", MK_DEVCFG1_WDTPS_16 >> 16);
			break;
		case MK_DEVCFG1_WDTPS_8:
			printf("                  %02x     Watchdog Timer Postscale 1:8\n", MK_DEVCFG1_WDTPS_8 >> 16);
			break;
		case MK_DEVCFG1_WDTPS_4:
			printf("                  %02x     Watchdog Timer Postscale 1:4\n", MK_DEVCFG1_WDTPS_4 >> 16);
			break;
		case MK_DEVCFG1_WDTPS_2:
			printf("                  %02x     Watchdog Timer Postscale 1:2\n", MK_DEVCFG1_WDTPS_2 >> 16);
			break;
		case MK_DEVCFG1_WDTPS_1:
			printf("                  %02x     Watchdog Timer Postscale 1:1\n", MK_DEVCFG1_WDTPS_1 >> 16);
			break;
		case MK_DEVCFG1_WDTPS_1048576:
		default:
			printf("                  %02x     Watchdog Timer Postscale 1:1048579\n", MK_DEVCFG1_WDTPS_1048576 >> 16);
			break;
	}

	switch (devcfg1 & MK_DEVCFG1_FCKSM_MASK){
		case MK_DEVCFG1_FCKSM_3:
			printf("                    %01x    Clock switching enabled, clock monitoring enabled\n", MK_DEVCFG1_FCKSM_3 >> 12);
			break;
		case MK_DEVCFG1_FCKSM_2:
			printf("                    %01x    Clock switching disabled, clock monitoring enabled\n", MK_DEVCFG1_FCKSM_2 >> 12);
			break;
		case MK_DEVCFG1_FCKSM_1:
			printf("                    %01x    Clock switching enabled, clock monitoring disabled\n", MK_DEVCFG1_FCKSM_1 >> 12);
			break;
		case MK_DEVCFG1_FCKSM_0:
			printf("                    %01x    Clock switching disabled, clock monitoring disabled\n", MK_DEVCFG1_FCKSM_0 >> 12);
			break;
	}
	
	if (devcfg1 & MK_DEVCFG1_OSCIOFNC){
		printf("                     %01x   CLKO output disabled\n", MK_DEVCFG1_OSCIOFNC >> 8);
	}
	else{
		printf("                     %01x   CLKO output active on OSC2\n", 0);
	}

	switch (devcfg1 & MK_DEVCFG1_POSCMOD_MASK){
		case MK_DEVCFG1_POSCMOD_DISABLED:
			printf("                     %01x   POSC disabled\n", MK_DEVCFG1_POSCMOD_DISABLED >> 8);
			break;
		case MK_DEVCFG1_POSCMOD_HS:
			printf("                     %01x   POSC set to HS Oscillator mode\n", MK_DEVCFG1_POSCMOD_HS >> 8);
			break;
		case MK_DEVCFG1_POSCMOD_RESERVED:
			printf("                     %01x   POSC - RESERVED setting\n", MK_DEVCFG1_POSCMOD_RESERVED >> 8);
			break;
		case MK_DEVCFG1_POSCMOD_EC:
			printf("                     %01x   POSC set to EC mode\n", MK_DEVCFG1_POSCMOD_EC >> 8);
			break;
	}	

	if (devcfg1 & MK_DEVCFG1_IESO){
		printf("                      %01x  Internal External Switchover enabled\n", MK_DEVCFG1_IESO >> 4);
	}
	else{
		printf("                      %01x  Internal External Switchover disabled\n", 0);
	}

	if (devcfg1 & MK_DEVCFG1_FSOSCEN){
		printf("                      %01x  SOSC enabled\n", MK_DEVCFG1_FSOSCEN >> 4);
	}
	else{
		printf("                      %01x  SOSC disabled\n", 0);
	}

	switch (devcfg1 & MK_DEVCFG1_DMTINV_MASK){
		case MK_DEVCFG1_DMTINV_127_128:
			printf("                      %02x Deadman Timer Window is 127/128 counter value\n", MK_DEVCFG1_DMTINV_127_128);
			break;
		case MK_DEVCFG1_DMTINV_63_64:
			printf("                      %02x Deadman Timer Window is 63/64 counter value\n", MK_DEVCFG1_DMTINV_63_64);
			break;
		case MK_DEVCFG1_DMTINV_31_32:
			printf("                      %02x Deadman Timer Window is 31/32 counter value\n", MK_DEVCFG1_DMTINV_31_32);
			break;
		case MK_DEVCFG1_DMTINV_15_16:
			printf("                      %02x Deadman Timer Window is 15/16 counter value\n", MK_DEVCFG1_DMTINV_15_16);
			break;
		case MK_DEVCFG1_DMTINV_7_8:
			printf("                      %02x Deadman Timer Window is 7/8 counter value\n", MK_DEVCFG1_DMTINV_7_8);
			break;
		case MK_DEVCFG1_DMTINV_3_4:
			printf("                      %02x Deadman Timer Window is 3/4 counter value\n", MK_DEVCFG1_DMTINV_3_4);
			break;
		case MK_DEVCFG1_DMTINV_1_2:
			printf("                      %02x Deadman Timer Window is 1/2 counter value\n", MK_DEVCFG1_DMTINV_1_2);
			break;
		case MK_DEVCFG1_DMTINV_0:
			printf("                      %02x Deadman Timer Window value is 0\n", MK_DEVCFG1_DMTINV_0);
			break;
	}	

	switch (devcfg1 & MK_DEVCFG1_FNOSC_MASK){
		case MK_DEVCFG1_FNOSC_LPRC:
			printf("                       %01x LPRC selected as Oscillator\n", MK_DEVCFG1_FNOSC_LPRC);
			break;
		case MK_DEVCFG1_FNOSC_SOSC:
			printf("                       %01x SOSC selected as Oscillator\n", MK_DEVCFG1_FNOSC_SOSC);
			break;
		case MK_DEVCFG1_FNOSC_USBPLL:
			printf("                       %01x USB PLL selected as Oscillator\n", MK_DEVCFG1_FNOSC_USBPLL);
			break;
		case MK_DEVCFG1_FNOSC_POSC:
			printf("                       %01x POSC selected as Oscillator\n", MK_DEVCFG1_FNOSC_POSC);
			break;
		case MK_DEVCFG1_FNOSC_SYSTEMPLL:
			printf("                       %01x System PLL selected as Oscillator\n", MK_DEVCFG1_FNOSC_SYSTEMPLL);
			break;
		case MK_DEVCFG1_FNOSC_FRC:
			printf("                       %01x FRC + divider selected as Oscillator\n", MK_DEVCFG1_FNOSC_FRC);
			break;
		default:
			printf("                       %01x RESERVED bit selected for Oscillator\n", MK_DEVCFG1_FNOSC_FRC);
			break;
	}	

}

void print_mk_devcfg0(uint32_t devcfg0, uint32_t alternate){
	// DEVCFG0
	if (PRIMARY == alternate){
		printf(" BF1DEVCFG0 = 0x%08X\n", devcfg0);
	}
	else{
		printf(" BF2DEVCFG0 = 0x%08X\n", devcfg0);
	}

	if (devcfg0 & MK_DEVCFG0_EJTAGBEN){
		printf("                %01x        Normal EJTAG functionality\n", MK_DEVCFG0_EJTAGBEN >> 28);
	}
	else{
		printf("                %01x        Reduced EJTAG functionality\n", 0);
	}

	if (devcfg0 & MK_DEVCFG0_POSCBOOST){
		printf("                  %01x      Boost the kick start of the POSC\n", MK_DEVCFG0_POSCBOOST >> 20);
	}
	else{
		printf("                  %01x      Normal start of the POSC\n", 0);
	}

	switch (devcfg0 & MK_DEVCFG0_POSCGAIN_MASK){
		case MK_DEVCFG0_POSCGAIN_3:
			printf("                  %02x     POSC gain level 3 (highest)\n", MK_DEVCFG0_POSCGAIN_3 >> 16);
			break;
		case MK_DEVCFG0_POSCGAIN_2:
			printf("                  %02x     POSC gain level 2\n", MK_DEVCFG0_POSCGAIN_2 >> 16);
			break;
		case MK_DEVCFG0_POSCGAIN_1:
			printf("                  %02x     POSC gain level 1\n", MK_DEVCFG0_POSCGAIN_1 >> 16);
			break;
		case MK_DEVCFG0_POSCGAIN_0:
			printf("                  %02x     POSC gain level 0 (lowest)\n", MK_DEVCFG0_POSCGAIN_0 >> 16);
			break;
	}

	if (devcfg0 & MK_DEVCFG0_SOSCBOOST){
		printf("                   %01x     Boost the kick start of the SOSC\n", MK_DEVCFG0_SOSCBOOST >> 16);
	}
	else{
		printf("                   %01x     Normal start of the SOSC\n", 0);
	}

	switch (devcfg0 & MK_DEVCFG0_SOSCGAIN_MASK){
		case MK_DEVCFG0_SOSCGAIN_3:
			printf("                   %01x     SOSC gain level 3 (highest)\n", MK_DEVCFG0_SOSCGAIN_3 >> 16);
			break;
		case MK_DEVCFG0_SOSCGAIN_2:
			printf("                   %01x     SOSC gain level 2\n", MK_DEVCFG0_SOSCGAIN_2 >> 16);
			break;
		case MK_DEVCFG0_SOSCGAIN_1:
			printf("                   %01x     SOSC gain level 1\n", MK_DEVCFG0_SOSCGAIN_1 >> 16);
			break;
		case MK_DEVCFG0_SOSCGAIN_0:
			printf("                   %01x     SOSC gain level 0 (lowest)\n", MK_DEVCFG0_SOSCGAIN_0 >> 16);
			break;
	}

	if (devcfg0 & MK_DEVCFG0_SMCLR){
		printf("                    %01x    MCLR pin generates normal system Reset\n", MK_DEVCFG0_SMCLR >> 12);
	}
	else{
		printf("                    %01x    MCLR pin generates a POR Reset\n", 0);
	}


	printf("                    %01x    Debug mode CPU Access Permission bits\n", (devcfg0 & MK_DEVCFG0_DBGPER_MASK) >> 12);
	if ((devcfg0 & MK_DEVCFG0_DBGPER_MASK) & MK_DEVCFG0_DBGPER_GRP2){
		printf("                         CPU access to Permissions Group 2: ALLOWED\n");
	}
	else{
		printf("                         CPU access to Permissions Group 2: DENIED\n");
	}
	if ((devcfg0 & MK_DEVCFG0_DBGPER_MASK) & MK_DEVCFG0_DBGPER_GRP1){
		printf("                         CPU access to Permissions Group 1: ALLOWED\n");
	}
	else{
		printf("                         CPU access to Permissions Group 1: DENIED\n");
	}
	if ((devcfg0 & MK_DEVCFG0_DBGPER_MASK) & MK_DEVCFG0_DBGPER_GRP0){
		printf("                         CPU access to Permissions Group 0: ALLOWED\n");
	}
	else{
		printf("                         CPU access to Permissions Group 0: DENIED\n");
	}

	if (devcfg0 & MK_DEVCFG0_FSLEEP){
		printf("                     %01x   Flash powered down in sleep mode\n", MK_DEVCFG0_FSLEEP >> 8);
	}
	else{
		printf("                     %01x   Flash power down controlled by VREGS bit\n", 0);
	}

	if (devcfg0 & MK_DEVCFG0_BOOTISA){
		printf("                      %01x  Boot and exception code is MIPS32\n", MK_DEVCFG0_BOOTISA >> 4);
	}
	else{
		printf("                      %01x  Boot and exception code is microMIPS\n", 0);
	}

	if (devcfg0 & MK_DEVCFG0_TRCEN){
		printf("                      %01x  Trace features enabled\n", MK_DEVCFG0_TRCEN >> 4);
	}
	else{
		printf("                      %01x  Trace features disabled\n", 0);
	}

	switch (devcfg0 & MK_DEVCFG0_ICESEL_MASK){
		case MK_DEVCFG0_ICESEL_1:
			printf("                      %02x PGEC1/PGED1 pair in use\n", MK_DEVCFG0_ICESEL_1);
			break;
		case MK_DEVCFG0_ICESEL_2:
			printf("                      %02x PGEC2/PGED2 pair in use\n", MK_DEVCFG0_SOSCGAIN_2);
			break;
		case MK_DEVCFG0_ICESEL_3:
			printf("                      %02x PGEC3/PGED3 pair in use\n", MK_DEVCFG0_SOSCGAIN_1);
			break;
		case MK_DEVCFG0_ICESEL_RESERVED:
			printf("                      %02x Reserved setting\n", MK_DEVCFG0_ICESEL_RESERVED);
			break;
	}

	if (devcfg0 & MK_DEVCFG0_JTAGEN){
		printf("                       %01x JTAG enabled\n", MK_DEVCFG0_JTAGEN);
	}
	else{
		printf("                       %01x JTAG disabled\n", 0);
	}

	switch (devcfg0 & MK_DEFCFG0_DEBUG_MASK){
		case MK_DEFCFG0_DEBUG_3:
			printf("                       %01x JTAG enabled, ICSP disabled, ICD disabled\n", MK_DEFCFG0_DEBUG_3);
			break;
		case MK_DEFCFG0_DEBUG_2:
			printf("                       %01x JTAG enabled, ICSP disabled, ICD enabled\n", MK_DEFCFG0_DEBUG_2);
			break;
		case MK_DEFCFG0_DEBUG_1:
			printf("                       %01x JTAG disabled, ICSP enabled, ICD disabled\n", MK_DEFCFG0_DEBUG_1);
			break;
		case MK_DEFCFG0_DEBUG_0:
			printf("                       %01x JTAG disabled, ICSP enabled, ICD enabled\n", MK_DEFCFG0_DEBUG_0);
			break;
	}

}

void print_mk_devcp(uint32_t devcp, uint32_t alternate){
	// DEVCP
	if (PRIMARY == alternate){
		printf(" BF1DEVCP0 = 0x%08X\n", devcp);
	}
	else{
		printf(" BF2DEVCP0 = 0x%08X\n", devcp);
	}

	if (devcp & MK_DEVCP0_CP){
		printf("               %01x         Code-protect disabled\n", MK_DEVCP0_CP >> 28);
	}
	else{
		printf("               %01x         Code protect enabled\n", 0);
	}
}

void print_mk_devsign(uint32_t devsign, uint32_t alternate){
	// DEVSIGN
	if (PRIMARY == alternate){
		printf(" BF1DEVSIGN0 = 0x%08X\n", devsign);
	}
	else{
		printf(" BF2DEVSIGN0 = 0x%08X\n", devsign);
	}
}

void print_mk_devseq(uint32_t devseq, uint32_t alternate){
	// DEVSEQ
	if (PRIMARY == alternate){
		printf(" BF1SEQ = 0x%08X\n", devseq);
	}
	else{
		printf(" BF2SEQ = 0x%08X\n", devseq);
	}

	printf("            %04x         CSEQ: Boot flash complement Sequence number\n", (devseq & 0xFFFF0000) >> 16);
	printf("                %04x     TSEQ: Boot flash true Sequence number\n", (devseq & 0xFFFF));
}
