#pragma once

#include <stdint.h>

typedef void (*xmodem_cb_t)(uint8_t* buf, uint32_t len);

int xmodemReceive(xmodem_cb_t cb);
