/* vi: set sw=4 ts=4: */
/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <stdio.h>
#include <string.h>

#include "pico/stdio.h"

#include "crc16.h"
#include "xcommon.h"
#include "xreceive.h"

static xmodem_cb_t callback = NULL;

int getbyte(uint32_t timeout) {
    int c = getchar_timeout_us(timeout * 1000);
    if (c == PICO_ERROR_TIMEOUT)
        return -1;
    return c;
}

void putCAN(void) {
    for (int n = 0; n < 3; n++)
        putchar(CAN);
}

static bool check(bool crc, const uint8_t* buf, int sz) {
    if (crc) {
        uint16_t crc = crc16_ccitt(buf, sz);
        uint16_t tcrc = (buf[sz] << 8) + buf[sz + 1];
        if (crc == tcrc)
            return true;
    } else {
        uint8_t cks = 0;
        for (int i = 0; i < sz; ++i)
            cks += buf[i];
        if (cks == buf[sz])
            return true;
    }
    return false;
}

void flushreceive(void) {
    while (getbyte(((DLY_1S)*3) >> 1) >= 0)
        ;
}

static uint8_t xbuff[1030]; /* 1024 for XModem 1k + 3 head chars + 2 crc + nul */

int xmodemReceive(xmodem_cb_t cb) {
    uint8_t* p;
    int bufsz;
    bool crc = false;
    uint8_t trychar = 'C';
    uint8_t packetno = 1;
    int i, c, len = 0;
    int retry, retrans = MAXRETRANS;

    callback = cb;

    for (;;) {
        for (retry = 0; retry < 16; ++retry) {
            if (trychar)
                putchar(trychar);
            if ((c = getbyte((DLY_1S)*2)) >= 0) {
                switch (c) {
                case SOH:
                    bufsz = 128;
                    goto start_recv;
                case STX:
                    bufsz = 1024;
                    goto start_recv;
                case EOT:
                    flushreceive();
                    putchar(ACK);
                    return len; /* normal end */
                case CAN:
                    if ((c = getbyte(DLY_1S)) == CAN) {
                        flushreceive();
                        putchar(ACK);
                        return -1; /* canceled by remote */
                    }
                    break;
                default:
                    break;
                }
            }
        }
        if (trychar == 'C') {
            trychar = NAK;
            continue;
        }
        flushreceive();
        putCAN();
        return -2; /* sync error */

    start_recv:
        if (trychar == 'C')
            crc = true;
        trychar = 0;
        p = xbuff;
        *p++ = c;
        for (i = 0; i < (bufsz + (crc ? 1 : 0) + 3); ++i) {
            if ((c = getbyte(DLY_1S)) < 0)
                goto reject;
            *p++ = c;
        }

        if (xbuff[1] == (uint8_t)(~xbuff[2]) &&
            (xbuff[1] == packetno || xbuff[1] == (uint8_t)packetno - 1) &&
            check(crc, &xbuff[3], bufsz)) {
            if (xbuff[1] == packetno) {
                callback(&xbuff[3], bufsz);
                ++packetno;
                retrans = MAXRETRANS + 1;
            }
            if (--retrans <= 0) {
                flushreceive();
                putCAN();
                return -3; /* too many retry error */
            }
            putchar(ACK);
            continue;
        }
    reject:
        flushreceive();
        putchar(NAK);
    }
}
