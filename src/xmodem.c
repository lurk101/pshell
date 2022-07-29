/* vi: set sw=4 ts=4: */
/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <stdio.h>
#include <string.h>

#include "crc16.h"
#include "xmodem.h"

#include "pico/stdio.h"

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
            return 1;
    } else {
        uint8_t cks = 0;
        for (int i = 0; i < sz; ++i)
            cks += buf[i];
        if (cks == buf[sz])
            return 1;
    }
    return 0;
}

void flushreceive(void) {
    while (getbyte(((DLY_1S)*3) >> 1) >= 0)
        ;
}

static uint8_t xbuff[1030]; /* 1024 for XModem 1k + 3 head chars + 2 crc + nul */

int xmodemReceive(xmodem_cb_t cb) {
    uint8_t* p;
    int bufsz;
    bool crc = 0;
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
            crc = 1;
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

int xmodemTransmit(uint8_t* src, int srcsz) {
    uint8_t xbuff[1030]; /* 1024 for XModem 1k + 3 head chars + 2 crc + nul */
    int bufsz, crc = -1;
    uint8_t packetno = 1;
    int i, c, len = 0;
    int retry;

    for (;;) {
        for (retry = 0; retry < 16; ++retry) {
            if ((c = getbyte((DLY_1S) << 1)) >= 0) {
                switch (c) {
                case 'C':
                    crc = 1;
                    goto start_trans;
                case NAK:
                    crc = 0;
                    goto start_trans;
                case CAN:
                    if ((c = getbyte(DLY_1S)) == CAN) {
                        putchar(ACK);
                        flushreceive();
                        return -1; /* canceled by remote */
                    }
                    break;
                default:
                    break;
                }
            }
        }
        putCAN();
        flushreceive();
        return -2; /* no sync */

        for (;;) {
        start_trans:
            xbuff[0] = SOH;
            bufsz = 128;
            xbuff[1] = packetno;
            xbuff[2] = ~packetno;
            c = srcsz - len;
            if (c > bufsz)
                c = bufsz;
            if (c >= 0) {
                memset(&xbuff[3], 0, bufsz);
                if (c == 0) {
                    xbuff[3] = CTRLZ;
                } else {
                    memcpy(&xbuff[3], &src[len], c);
                    if (c < bufsz)
                        xbuff[3 + c] = CTRLZ;
                }
                if (crc) {
                    uint16_t ccrc = crc16_ccitt(&xbuff[3], bufsz);
                    xbuff[bufsz + 3] = (ccrc >> 8) & 0xFF;
                    xbuff[bufsz + 4] = ccrc & 0xFF;
                } else {
                    uint8_t ccks = 0;
                    for (i = 3; i < bufsz + 3; ++i) {
                        ccks += xbuff[i];
                    }
                    xbuff[bufsz + 3] = ccks;
                }
                for (retry = 0; retry < MAXRETRANS; ++retry) {
                    for (i = 0; i < bufsz + 4 + (crc ? 1 : 0); ++i) {
                        putchar(xbuff[i]);
                    }
                    if ((c = getbyte(DLY_1S)) >= 0) {
                        switch (c) {
                        case ACK:
                            ++packetno;
                            len += bufsz;
                            goto start_trans;
                        case CAN:
                            if ((c = getbyte(DLY_1S)) == CAN) {
                                putchar(ACK);
                                flushreceive();
                                return -1; /* canceled by remote */
                            }
                            break;
                        case NAK:
                        default:
                            break;
                        }
                    }
                }
                putCAN();
                flushreceive();
                return -4; /* xmit error */
            } else {
                for (retry = 0; retry < 10; ++retry) {
                    putchar(EOT);
                    if ((c = getbyte((DLY_1S) << 1)) == ACK)
                        break;
                }
                flushreceive();
                return (c == ACK) ? len : -5;
            }
        }
    }
}
