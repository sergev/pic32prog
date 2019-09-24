/*
 * Interface to serial port.
 *
 * Copyright (C) 2015 Serge Vakulenko
 *
 * This file is part of PIC32PROG project, which is distributed
 * under the terms of the GNU General Public License (GPL).
 * See the accompanying file "COPYING" for more details.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "adapter.h"

#if defined(__WIN32__) || defined(WIN32)
    #include <windows.h>
    #include <malloc.h>
    static void *fd = INVALID_HANDLE_VALUE;
    static DCB saved_mode;
#else
    #include <termios.h>
    static int fd = -1;
    static struct termios saved_mode;
#endif

/*
 * Encode the speed in bits per second into bit value
 * accepted by cfsetspeed() function.
 * Return -1 when speed is not supported.
 */
static int baud_encode(int bps)
{
#if defined(__WIN32__) || defined(WIN32) || B115200 == 115200
    /* Windows, BSD, Mac OS: any baud rate allowed. */
    return bps;
#else
    /* Linux: only a limited set of values permitted. */
    switch (bps) {
#ifdef B75
    case 75: return B75;
#endif
#ifdef B110
    case 110: return B110;
#endif
#ifdef B134
    case 134: return B134;
#endif
#ifdef B150
    case 150: return B150;
#endif
#ifdef B200
    case 200: return B200;
#endif
#ifdef B300
    case 300: return B300;
#endif
#ifdef B600
    case 600: return B600;
#endif
#ifdef B1200
    case 1200: return B1200;
#endif
#ifdef B1800
    case 1800: return B1800;
#endif
#ifdef B2400
    case 2400: return B2400;
#endif
#ifdef B4800
    case 4800: return B4800;
#endif
#ifdef B9600
    case 9600: return B9600;
#endif
#ifdef B19200
    case 19200: return B19200;
#endif
#ifdef B38400
    case 38400: return B38400;
#endif
#ifdef B57600
    case 57600: return B57600;
#endif
#ifdef B115200
    case 115200: return B115200;
#endif
#ifdef B230400
    case 230400: return B230400;
#endif
#ifdef B460800
    case 460800: return B460800;
#endif
#ifdef B500000
    case 500000: return B500000;
#endif
#ifdef B576000
    case 576000: return B576000;
#endif
#ifdef B921600
    case 921600: return B921600;
#endif
#ifdef B1000000
    case 1000000: return B1000000;
#endif
#ifdef B1152000
    case 1152000: return B1152000;
#endif
#ifdef B1500000
    case 1500000: return B1500000;
#endif
#ifdef B2000000
    case 2000000: return B2000000;
#endif
#ifdef B2500000
    case 2500000: return B2500000;
#endif
#ifdef B3000000
    case 3000000: return B3000000;
#endif
#ifdef B3500000
    case 3500000: return B3500000;
#endif
#ifdef B4000000
    case 4000000: return B4000000;
#endif
    }
printf("Unknown baud\n");
    return -1;
#endif
}

/*
 * Check whether the given speed in bits per second
 * is supported by the system.
 * Return 0 when not supported.
 */
int serial_speed_valid(int bps)
{
    return baud_encode(bps) > 0;
}

/*
 * Send data to device.
 * Return number of bytes, or -1 on error.
 */
int serial_write(unsigned char *data, int len)
{
#if defined(__WIN32__) || defined(WIN32)
    DWORD written;

    if (! WriteFile(fd, data, len, &written, 0))
        return -1;
    return written;
#else
    return write(fd, data, len);
#endif
}

/*
 * Receive data from device.
 * Return number of bytes, or -1 on error.
 */
int serial_read(unsigned char *data, int len, int timeout_msec)
{
#if defined(__WIN32__) || defined(WIN32)
    DWORD got;
    COMMTIMEOUTS ctmo;

    /* Reset the Windows RX timeout to the current timeout_msec
     * value, as it may have changed since the last read.
     */
    memset(&ctmo, 0, sizeof(ctmo));
    ctmo.ReadIntervalTimeout = 0;
    ctmo.ReadTotalTimeoutMultiplier = 0;
    ctmo.ReadTotalTimeoutConstant = timeout_msec;
    if (! SetCommTimeouts(fd, &ctmo)) {
        fprintf(stderr, "Cannot set timeouts in serial_read()\n");
        return -1;
    }

    if (! ReadFile(fd, data, len, &got, 0)) {
        fprintf(stderr, "serial_read: read error\n");
        exit(-1);
    }
#else
    struct timeval timeout, to2;
    long got;
    fd_set rfds;

    timeout.tv_sec = timeout_msec / 1000;
    timeout.tv_usec = timeout_msec % 1000 * 1000;
again:
    to2 = timeout;
    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);

    got = select(fd + 1, &rfds, 0, 0, &to2);
    if (got < 0) {
        if (errno == EINTR || errno == EAGAIN) {
            if (debug_level > 1)
                printf("serial_read: retry on select\n");
            goto again;
        }
        fprintf(stderr, "serial_read: select error: %s\n", strerror(errno));
        exit(-1);
    }
#endif
    if (got == 0) {
        if (debug_level > 1)
            printf("serial_read: no characters to read\n");
        return 0;
    }

#if ! defined(__WIN32__) && ! defined(WIN32)
    got = read(fd, data, (len > 1024) ? 1024 : len);
    if (got < 0) {
        fprintf(stderr, "serial_read: read error\n");
        exit(-1);
    }
#endif
    return got;
}

/*
 * Close the serial port.
 */
void serial_close()
{
#if defined(__WIN32__) || defined(WIN32)
    SetCommState(fd, &saved_mode);
    CloseHandle(fd);
#else
    tcsetattr(fd, TCSANOW, &saved_mode);
    close(fd);
#endif
}

/*
 * Open the serial port.
 * Return -1 on error.
 */
int serial_open(const char *devname, int baud_rate)
{
#if defined(__WIN32__) || defined(WIN32)
    DCB new_mode;
#else
    struct termios new_mode;
#endif

#if defined(__WIN32__) || defined(WIN32)
    /* Check for the Windows device syntax and bend a DOS device
     * into that syntax to allow higher COM numbers than 9
     */
    if (devname[0] != '\\') {
        // Prepend device prefix: COM23 -> \\.\COM23
        char *buf = alloca(5 + strlen(devname));
        if (! buf) {
            fprintf(stderr, "%s: Out of memory\n", devname);
            return -1;
        }
        strcpy(buf, "\\\\.\\");
        strcat(buf, devname);
        devname = buf;
    }

    /* Open port */
    fd = CreateFile(devname, GENERIC_READ | GENERIC_WRITE,
        0, 0, OPEN_EXISTING, 0, 0);
    if (fd == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "%s: Cannot open\n", devname);
        return -1;
    }

    /* Set serial attributes */
    memset(&saved_mode, 0, sizeof(saved_mode));
    if (! GetCommState(fd, &saved_mode)) {
        fprintf(stderr, "%s: Cannot get state\n", devname);
        return -1;
    }

    new_mode = saved_mode;

    new_mode.fDtrControl = DTR_CONTROL_ENABLE;
    new_mode.BaudRate = baud_rate;
    new_mode.ByteSize = 8;
    new_mode.StopBits = ONESTOPBIT;
    new_mode.Parity = 0;
    new_mode.fParity = FALSE;
    new_mode.fOutX = FALSE;
    new_mode.fInX = FALSE;
    new_mode.fOutxCtsFlow = FALSE;
    new_mode.fOutxDsrFlow = FALSE;
    new_mode.fRtsControl = RTS_CONTROL_ENABLE;
    new_mode.fNull = FALSE;
    new_mode.fAbortOnError = FALSE;
    new_mode.fBinary = TRUE;
    if (! SetCommState(fd, &new_mode)) {
        fprintf(stderr, "%s: Cannot set state\n", devname);
        return -1;
    }
#else
    /* Encode baud rate. */
    int baud_code = baud_encode(baud_rate);
    if (baud_code < 0) {
        fprintf(stderr, "%s: Bad baud rate %d\n", devname, baud_rate);
        return -1;
    }

    /* Open port */
    fd = open(devname, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) {
        perror(devname);
        return -1;
    }

    /* Set serial attributes */
    memset(&saved_mode, 0, sizeof(saved_mode));
    tcgetattr(fd, &saved_mode);

    /* 8n1, ignore parity */
    memset(&new_mode, 0, sizeof(new_mode));
    new_mode.c_cflag = CS8 | CLOCAL | CREAD;
    new_mode.c_iflag = IGNBRK;
    new_mode.c_oflag = 0;
    new_mode.c_lflag = 0;
    new_mode.c_cc[VTIME] = 0;
    new_mode.c_cc[VMIN]  = 1;
    cfsetispeed(&new_mode, baud_code);
    cfsetospeed(&new_mode, baud_code);
    tcflush(fd, TCIFLUSH);
    tcsetattr(fd, TCSANOW, &new_mode);

    /* Clear O_NONBLOCK flag. */
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags >= 0)
        fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
#endif
    return 0;
}

/*
 * Change baud rate
 * Return -1 on error.
 */
int serial_baud(int baud_rate)
{
#if defined(__WIN32__) || defined(WIN32)
    DCB new_mode;
#else
    struct termios new_mode;
#endif

#if defined(__WIN32__) || defined(WIN32)
    new_mode = saved_mode;

    new_mode.BaudRate = baud_rate;
    new_mode.ByteSize = 8;
    new_mode.StopBits = ONESTOPBIT;
    new_mode.Parity = 0;
    new_mode.fParity = FALSE;
    new_mode.fOutX = FALSE;
    new_mode.fInX = FALSE;
    new_mode.fOutxCtsFlow = FALSE;
    new_mode.fOutxDsrFlow = FALSE;
    new_mode.fRtsControl = RTS_CONTROL_ENABLE;
    new_mode.fNull = FALSE;
    new_mode.fAbortOnError = FALSE;
    new_mode.fBinary = TRUE;
    if (! SetCommState(fd, &new_mode)) {
        fprintf(stderr, "Cannot set state\n");
        return -1;
    }
#else
    /* Encode baud rate. */
    int baud_code = baud_encode(baud_rate);
    if (baud_code < 0) {
        fprintf(stderr, "Bad baud rate %d\n", baud_rate);
        return -1;
    }

    /* 8n1, ignore parity */
    memset(&new_mode, 0, sizeof(new_mode));
    new_mode.c_cflag = CS8 | CLOCAL | CREAD;
    new_mode.c_iflag = IGNBRK;
    new_mode.c_oflag = 0;
    new_mode.c_lflag = 0;
    new_mode.c_cc[VTIME] = 0;
    new_mode.c_cc[VMIN]  = 1;
    cfsetispeed(&new_mode, baud_code);
    cfsetospeed(&new_mode, baud_code);
    tcflush(fd, TCIFLUSH);
    tcsetattr(fd, TCSANOW, &new_mode);

    /* Clear O_NONBLOCK flag. */
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags >= 0)
        fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
#endif
    return 0;
}
