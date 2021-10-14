/* vi: set sw=4 ts=4: */
/* SPDX-License-Identifier: GPL-3.0-or-later */

#pragma once

#define SOH 0x01
#define STX 0x02
#define EOT 0x04
#define ACK 0x06
#define NAK 0x15
#define CAN 0x18
#define CTRLZ 0x1a

#define DLY_1S 1000
#define MAXRETRANS 25

int getbyte(uint32_t timeout);
void flushreceive(void);
void putCAN(void);
