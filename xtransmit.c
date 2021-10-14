
#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"

#include "crc16.h"
#include "xcommon.h"
#include "xtransmit.h"

int xmodemTransmit(unsigned char* src, int srcsz) {
    unsigned char xbuff[1030]; /* 1024 for XModem 1k + 3 head chars + 2 crc + nul */
    int bufsz, crc = -1;
    unsigned char packetno = 1;
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
                    unsigned short ccrc = crc16_ccitt(&xbuff[3], bufsz);
                    xbuff[bufsz + 3] = (ccrc >> 8) & 0xFF;
                    xbuff[bufsz + 4] = ccrc & 0xFF;
                } else {
                    unsigned char ccks = 0;
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
