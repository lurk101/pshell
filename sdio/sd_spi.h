/* sd_spi.h
Copyright 2021 Carl John Kugler III

Licensed under the Apache License, Version 2.0 (the License); you may not use
this file except in compliance with the License. You may obtain a copy of the
License at

   http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software distributed
under the License is distributed on an AS IS BASIS, WITHOUT WARRANTIES OR
CONDITIONS OF ANY KIND, either express or implied. See the License for the
specific language governing permissions and limitations under the License.
*/

#ifndef _SD_SPI_H_
#define _SD_SPI_H_

#include "sd_card.h"
#include <stdint.h>

bool sd_spi_transfer(sd_card_t* pSD, const uint8_t* tx, uint8_t* rx, size_t length);
uint8_t sd_spi_write(sd_card_t* pSD, const uint8_t value);
void sd_spi_deselect_pulse(sd_card_t* pSD);
void sd_spi_go_low_frequency(sd_card_t* this);
void sd_spi_go_high_frequency(sd_card_t* this);
void sd_spi_acquire(sd_card_t* pSD);
void sd_spi_release(sd_card_t* pSD);
void sd_spi_send_initializing_sequence(sd_card_t* pSD);

#endif
