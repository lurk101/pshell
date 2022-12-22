#pragma once

#define STA_NOINIT 1
#define STA_NODISK 2

#include "sd_card.h"

spi_t* spi_get();
sd_card_t* sd_get();
