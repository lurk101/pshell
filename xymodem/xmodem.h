/* vi: set sw=4 ts=4: */
/* SPDX-License-Identifier: GPL-3.0-or-later */

#pragma once

#include <stdint.h>

typedef int (*xmodem_cb_t)(uint8_t* buf, uint32_t len);

int xmodemReceive(xmodem_cb_t cb);

int xmodemTransmit(xmodem_cb_t cb);
