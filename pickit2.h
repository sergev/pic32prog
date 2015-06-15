/*
 * Microchip PICkit2 USB adapter.
 * Low-level interface.
 *
 * Copyright (C) 2011-2013 Serge Vakulenko
 *
 * This file is part of PIC32PROG project, which is distributed
 * under the terms of the GNU General Public License (GPL).
 * See the accompanying file "COPYING" for more details.
 */

#ifndef _PICKIT2_H
#define _PICKIT2_H

/*
 * PICkit2 Commands.
 */
#define CMD_NO_OPERATION           0x5A     // Do nothing
#define CMD_GET_VERSION            0x76     // {major} {minor} {dot}
                                            // Get firmware version
#define CMD_BOOT_MODE              0x42     // Enter Bootloader mode
#define CMD_SET_VDD                0xA0     // {CCPL} {CCPH} {VDDLim}
#define CMD_SET_VPP                0xA1     // {CCPR2L} {VPPADC} {VPPLim}
#define CMD_READ_STATUS            0xA2     // {StsL} {StsH}
#define CMD_READ_VOLTAGES          0xA3     // {VddL} {VddH} {VppL} {VppH}
#define CMD_DOWNLOAD_SCRIPT        0xA4     // {Script#} {ScriptLengthN} {Script1} {Script2} ... {ScriptN}
                                            // Store a script in the Script Buffer
#define CMD_RUN_SCRIPT             0xA5     // {Script#} {iterations}
                                            // Run a script from the script buffer
#define CMD_EXECUTE_SCRIPT         0xA6     // {ScriptLengthN} {Script1} {Script2} ... {ScriptN}
                                            // Immediately execute the included script
#define CMD_CLEAR_DOWNLOAD_BUFFER  0xA7     // Empty the download buffer
#define CMD_DOWNLOAD_DATA          0xA8     // {DataLength} {Data1} {Data2} ... {DataN}
                                            // Add data to download buffer
#define CMD_CLEAR_UPLOAD_BUFFER    0xA9     // Empty the upload buffer
#define CMD_UPLOAD_DATA            0xAA     // {DataLengthN} {data1} {data2} ... {dataN}
                                            // Read data from upload buffer
#define CMD_CLEAR_SCRIPT_BUFFER    0xAB
#define CMD_UPLOAD_DATA_NOLEN      0xAC     // {Data1} {Data2} ... {DataN}
                                            // Read data from upload buffer
#define CMD_END_OF_BUFFER          0xAD     // Skip the rest of commands
#define CMD_RESET                  0xAE     // Reset
#define CMD_SCRIPT_BUFFER_CSUM     0xAF     // {LenSumL} {LenSumH} {BufSumL} {BufSumH}
                                            // Calculate checksums of the Script Buffer
#define CMD_SET_VOLTAGE_CAL        0xB0     // {adc_calfactorL} {adc_calfactorH} {vdd_offset} {calfactor}
#define CMD_WRITE_INTERNAL_EEPROM  0xB1     // {address} {datalength} {data1} {data2} ... {dataN}
                                            // Write data to PIC18F2550 EEPROM
#define CMD_READ_INTERNAL_EEPROM   0xB2     // {address} {datalength}
                                            // {data1} {data2} ... {dataN}
                                            // Read bytes from PIC18F2550 EEPROM
#define CMD_ENTER_UART_MODE        0xB3
#define CMD_EXIT_UART_MODE         0xB4     // Exits the firmware from UART Mode
#define CMD_ENTER_LEARN_MODE       0xB5     // {0x50} {0x4B} {0x32} {EEsize}
                                            // Puts the firmware in PK2GO Learn Mode
#define CMD_EXIT_LEARN_MODE        0xB6     // Ignore
#define CMD_ENABLE_PK2GO_MODE      0xB7     // {0x50} {0x4B} {0x32} {EEsize}
                                            // Puts the firmware in PK2GO Mode
#define CMD_LOGIC_ANALYZER_GO      0xB8     // {EdgeRising} {TrigMask} {TrigStates} {EdgeMask} {TrigCount} {PostTrigCountL} {PostTrigCountH} {SampleRateFactor}
                                            // {TrigLocL} {TrigLocH}
#define CMD_COPY_RAM_UPLOAD        0xB9     // {StartAddrL} {StartAddrH}

/*
 * PICkit3 Commands.
 */
#define CMD_GETVERSIONS_MPLAB      0x41     // Get firmware version

/*
 * Status bits.
 */
#define STATUS_VDD_GND_ON          0x0001   // Vdd GND On
#define STATUS_VDD_ON              0x0002   // Vdd On
#define STATUS_VPP_GND_ON          0x0004   // Vpp GND On
#define STATUS_VPP_ON              0x0008   // Vpp On
#define STATUS_VDD_ERROR           0x0010   // Vdd Error
#define STATUS_VPP_ERROR           0x0020   // Vpp Error
#define STATUS_BUTTON_PRESSED      0x0040   // Button Pressed
#define STATUS_RESET               0x0100   // Reset
#define STATUS_UART_MODE           0x0200   // UART Mode
#define STATUS_ICD_TIMEOUT         0x0400   // ICD TimeOut
#define STATUS_UPLOAD_FULL         0x0800   // Upload Full
#define STATUS_DOWNLOAD_EMPTY      0x1000   // Download Empty
#define STATUS_EMPTY_SCRIPT        0x2000   // Empty Script
#define STATUS_SCRIPT_BUF_OVFL     0x4000   // Script Buffer Overflow
#define STATUS_DOWNLOAD_OVFL       0x8000   // Download Overflow

/*
 * Script instructions.
 */
#define SCRIPT_JT2_PE_PROG_RESP    0xB3     // +
#define SCRIPT_JT2_WAIT_PE_RESP    0xB4     // +
#define SCRIPT_JT2_GET_PE_RESP     0xB5     // +
#define SCRIPT_JT2_XFERINST_BUF    0xB6     //
#define SCRIPT_JT2_XFRFASTDAT_BUF  0xB7     // +
#define SCRIPT_JT2_XFRFASTDAT_LIT  0xB8     // + 4
#define SCRIPT_JT2_XFERDATA32_LIT  0xB9     // + 4
#define SCRIPT_JT2_XFERDATA8_LIT   0xBA     // + 1
#define SCRIPT_JT2_SENDCMD         0xBB     // + 1
#define SCRIPT_JT2_SETMODE         0xBC     // + 2
#define SCRIPT_UNIO_TX_RX          0xBD     //
#define SCRIPT_UNIO_TX             0xBE     //
#define SCRIPT_MEASURE_PULSE       0xBF     //
#define SCRIPT_ICDSLAVE_TX_BUF_BL  0xC0     //
#define SCRIPT_ICDSLAVE_TX_LIT_BL  0xC1     //
#define SCRIPT_ICDSLAVE_RX_BL      0xC2     //
#define SCRIPT_SPI_RDWR_BYTE_BUF   0xC3     //
#define SCRIPT_SPI_RDWR_BYTE_LIT   0xC4     //
#define SCRIPT_SPI_RD_BYTE_BUF     0xC5     //
#define SCRIPT_SPI_WR_BYTE_BUF     0xC6     //
#define SCRIPT_SPI_WR_BYTE_LIT     0xC7     //
#define SCRIPT_I2C_RD_BYTE_NACK    0xC8     //
#define SCRIPT_I2C_RD_BYTE_ACK     0xC9     //
#define SCRIPT_I2C_WR_BYTE_BUF     0xCA     //
#define SCRIPT_I2C_WR_BYTE_LIT     0xCB     //
#define SCRIPT_I2C_STOP            0xCC     //
#define SCRIPT_I2C_START           0xCD     //
#define SCRIPT_AUX_STATE_BUFFER    0xCE     //
#define SCRIPT_SET_AUX             0xCF     //
#define SCRIPT_WRITE_BITS_BUF_HLD  0xD0     //
#define SCRIPT_WRITE_BITS_LIT_HLD  0xD1     //
#define SCRIPT_CONST_WRITE_DL      0xD2     //
#define SCRIPT_WRITE_BUFBYTE_W     0xD3     //
#define SCRIPT_WRITE_BUFWORD_W     0xD4     //
#define SCRIPT_RD2_BITS_BUFFER     0xD5     //
#define SCRIPT_RD2_BYTE_BUFFER     0xD6     //
#define SCRIPT_VISI24              0xD7     //
#define SCRIPT_NOP24               0xD8     //
#define SCRIPT_COREINST24          0xD9     //
#define SCRIPT_COREINST18          0xDA     //
#define SCRIPT_POP_DOWNLOAD        0xDB     //
#define SCRIPT_ICSP_STATES_BUFFER  0xDC     //
#define SCRIPT_LOOPBUFFER          0xDD     //
#define SCRIPT_ICDSLAVE_TX_BUF     0xDE     //
#define SCRIPT_ICDSLAVE_TX_LIT     0xDF     //
#define SCRIPT_ICDSLAVE_RX         0xE0     //
#define SCRIPT_POKE_SFR            0xE1     //
#define SCRIPT_PEEK_SFR            0xE2     //
#define SCRIPT_EXIT_SCRIPT         0xE3     //
#define SCRIPT_GOTO_INDEX          0xE4     //
#define SCRIPT_IF_GT_GOTO          0xE5     //
#define SCRIPT_IF_EQ_GOTO          0xE6     //
#define SCRIPT_DELAY_SHORT         0xE7     // + 1 increments of 42.7us
#define SCRIPT_DELAY_LONG          0xE8     // + 1 increments of 5.46ms
#define SCRIPT_LOOP                0xE9     // + 2
#define SCRIPT_SET_ICSP_SPEED      0xEA     // + 1
#define SCRIPT_READ_BITS           0xEB     //
#define SCRIPT_READ_BITS_BUFFER    0xEC     //
#define SCRIPT_WRITE_BITS_BUFFER   0xED     //
#define SCRIPT_WRITE_BITS_LITERAL  0xEE     //
#define SCRIPT_READ_BYTE           0xEF     //
#define SCRIPT_READ_BYTE_BUFFER    0xF0     //
#define SCRIPT_WRITE_BYTE_BUFFER   0xF1     //
#define SCRIPT_WRITE_BYTE_LITERAL  0xF2     // + 1
#define SCRIPT_SET_ICSP_PINS       0xF3     // + 1
#define SCRIPT_BUSY_LED_OFF        0xF4     // +
#define SCRIPT_BUSY_LED_ON         0xF5     // +
#define SCRIPT_MCLR_GND_OFF        0xF6     // +
#define SCRIPT_MCLR_GND_ON         0xF7     // +
#define SCRIPT_VPP_PWM_OFF         0xF8     // +
#define SCRIPT_VPP_PWM_ON          0xF9     // +
#define SCRIPT_VPP_OFF             0xFA     // +
#define SCRIPT_VPP_ON              0xFB     // +
#define SCRIPT_VDD_GND_OFF         0xFC     // +
#define SCRIPT_VDD_GND_ON          0xFD     // +
#define SCRIPT_VDD_OFF             0xFE     // +
#define SCRIPT_VDD_ON              0xFF     // +

#endif
