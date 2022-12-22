#include <string.h>
//
#include "hw_config.h"
//
void spi_dma_isr();

// Hardware Configuration of SPI "objects"
// Note: multiple SD cards can be driven by one SPI if they use different slave
// selects.
static spi_t spis[] = { // One for each SPI.
    {.hw_inst = spi1,   // SPI component
     .miso_gpio = 19,   // GPIO number (not pin number)
     .mosi_gpio = 18,
     .sck_gpio = 5,
     .set_drive_strength = true,
     .mosi_gpio_drive_strength = GPIO_DRIVE_STRENGTH_2MA,
     .sck_gpio_drive_strength = GPIO_DRIVE_STRENGTH_2MA,

     // .baud_rate = 1000 * 1000,
     //.baud_rate = 12500 * 1000,  // The limitation here is SPI slew rate.
     .baud_rate = 25 * 1000 * 1000, // Actual frequency: 20833333. Has
     // worked for me with SanDisk.

     .dma_isr = spi_dma_isr}};

// Hardware Configuration of the SD Card "objects"
static sd_card_t sd_cards[] = { // One for each SD card
    {.spi = &spis[0],           // Pointer to the SPI driving this card
     .ss_gpio = 22,             // The SPI slave select GPIO for this SD card
     .set_drive_strength = true,
     .ss_gpio_drive_strength = GPIO_DRIVE_STRENGTH_2MA,
     //.use_card_detect = false,

     // State variables:
     .m_Status = STA_NOINIT}};

void spi_dma_isr() { spi_irq_handler(&spis[0]); }

sd_card_t* sd_get() { return &sd_cards[0]; }

spi_t* spi_get() { return &spis[0]; }
