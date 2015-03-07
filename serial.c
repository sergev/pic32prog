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
    static void *fd = INVALID_HANDLE_VALUE;
    static DCB saved_mode;
#else
    #include <termios.h>
    static int fd = -1;
    static struct termios saved_mode;
#endif

/*
 * Send data to device.
 * Return number of bytes, or -1 on error.
 */
int serial_write (unsigned char *data, int len)
{
#if defined(__WIN32__) || defined(WIN32)
    DWORD written;

    if (! WriteFile (fd, data, len, &written, 0))
        return -1;
    return len;
#else
    return write (fd, data, len);
#endif
}

/*
 * Receive data from device.
 * Return number of bytes, or -1 on error.
 */
int serial_read (unsigned char *data, int len)
{
#if defined(__WIN32__) || defined(WIN32)
    DWORD got;

    if (! ReadFile (fd, data, len, &got, 0)) {
        fprintf (stderr, "stk-send: read error\n");
        exit (-1);
    }
#else
    struct timeval timeout, to2;
    long got;
    fd_set rfds;

    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    to2 = timeout;
again:
    FD_ZERO (&rfds);
    FD_SET (fd, &rfds);

    got = select (fd + 1, &rfds, 0, 0, &to2);
    if (got < 0) {
        if (errno == EINTR || errno == EAGAIN) {
            if (debug_level > 1)
                printf ("stk-send: programmer is not responding\n");
            goto again;
        }
        fprintf (stderr, "stk-send: select error: %s\n", strerror (errno));
        exit (1);
    }
#endif
    if (got == 0) {
        if (debug_level > 1)
            printf ("stk-send: programmer is not responding\n");
        return 0;
    }

#if ! defined(__WIN32__) && !defined(WIN32)
    got = read (fd, data, (len > 1024) ? 1024 : len);
    if (got < 0) {
        fprintf (stderr, "stk-send: read error\n");
        exit (-1);
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
    SetCommState (fd, &saved_mode);
    CloseHandle (fd);
#else
    tcsetattr (fd, TCSANOW, &saved_mode);
    close (fd);
#endif
}

/*
 * Open the serial port.
 * Return -1 on error.
 */
int serial_open (const char *devname)
{
#if defined(__WIN32__) || defined(WIN32)
    DCB new_mode;
    COMMTIMEOUTS ctmo;
#else
    struct termios new_mode;
#endif

#if defined(__WIN32__) || defined(WIN32)
    /* Open port */
    fd = CreateFile (devname, GENERIC_READ | GENERIC_WRITE,
        0, 0, OPEN_EXISTING, 0, 0);
    if (fd == INVALID_HANDLE_VALUE) {
        fprintf (stderr, "%s: Cannot open\n", devname);
        return -1;
    }

    /* Set serial attributes */
    memset (&saved_mode, 0, sizeof(saved_mode));
    if (! GetCommState (fd, &saved_mode)) {
        fprintf (stderr, "%s: Cannot get state\n", devname);
        return -1;
    }

    new_mode = saved_mode;

    new_mode.BaudRate = CBR_115200;
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
    if (! SetCommState (fd, &new_mode)) {
        fprintf (stderr, "%s: Cannot set state\n", devname);
        return -1;
    }

    memset (&ctmo, 0, sizeof(ctmo));
    ctmo.ReadIntervalTimeout = 0;
    ctmo.ReadTotalTimeoutMultiplier = 0;
    ctmo.ReadTotalTimeoutConstant = 5000;
    if (! SetCommTimeouts (fd, &ctmo)) {
        fprintf (stderr, "%s: Cannot set timeouts\n", devname);
        return -1;
    }
#else
    /* Open port */
    fd = open (devname, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) {
        perror (devname);
        return -1;
    }

    /* Set serial attributes */
    memset (&saved_mode, 0, sizeof(saved_mode));
    tcgetattr (fd, &saved_mode);

    /* 8n1, ignore parity */
    memset (&new_mode, 0, sizeof(new_mode));
    new_mode.c_cflag = CS8 | CLOCAL | CREAD;
    new_mode.c_iflag = IGNBRK;
    new_mode.c_oflag = 0;
    new_mode.c_lflag = 0;
    new_mode.c_cc[VTIME] = 0;
    new_mode.c_cc[VMIN]  = 1;
    cfsetispeed (&new_mode, B115200);
    cfsetospeed (&new_mode, B115200);
    tcflush (fd, TCIFLUSH);
    tcsetattr (fd, TCSANOW, &new_mode);

    /* Clear O_NONBLOCK flag. */
    int flags = fcntl (fd, F_GETFL, 0);
    if (flags >= 0)
        fcntl (fd, F_SETFL, flags & ~O_NONBLOCK);
#endif
    return 0;
}
