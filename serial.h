/*
 * Interface to serial port.
 *
 * Copyright (C) 2015 Serge Vakulenko
 *
 * This file is part of PIC32PROG project, which is distributed
 * under the terms of the GNU General Public License (GPL).
 * See the accompanying file "COPYING" for more details.
 */

#ifndef _SERIAL_H
#define _SERIAL_H

/*
 * Open the serial port with the specified baud rate and
 * receive timeout (in milliseconds).
 * Return -1 on error.
 */
int serial_open (const char *devname, int baud_rate, int timeout);

/*
 * Change the serial baud rate
 * Return -1 on error.
 */
int serial_baud (int baud_rate);

/*
 * Close the serial port.
 */
void serial_close (void);

/*
 * Send data to device.
 * Return number of bytes, or -1 on error.
 */
int serial_write (unsigned char *data, int len);

/*
 * Receive data from device.
 * Return number of bytes, or -1 on error.
 */
int serial_read (unsigned char *data, int len);

/*
 * Check whether the given speed in bits per second
 * is supported by the system.
 * Return 0 when not supported.
 */
int serial_speed_valid (int bps);

#endif
