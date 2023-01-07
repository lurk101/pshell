/* sp_sdi.h - SPI routines to access SD card */

#ifndef SD_SPI_H
#define SD_SPI_H

#include <stdbool.h>
#include <stdint.h>

typedef enum { sdtpUnk, sdtpVer1, sdtpVer2, sdtpHigh } SD_TYPE;
extern SD_TYPE sd_type;

bool sd_spi_init(void);
int sd_spi_sectors(void);
void sd_spi_term(void);
bool sd_spi_read(uint32_t lba, uint8_t* buff);
bool sd_spi_write(uint32_t lba, const uint8_t* buff);

#endif
