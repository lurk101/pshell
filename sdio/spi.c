/* spi.c
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

#include <stdbool.h>
//
#include "pico/stdlib.h"
//
#include "spi.h"

static bool irqChannel1 = false;
static bool irqShared = true;

void spi_irq_handler(spi_t* pSPI) {
    if (irqChannel1) {
        if (dma_hw->ints1 & 1u << pSPI->rx_dma) { // Ours?
            dma_hw->ints1 = 1u << pSPI->rx_dma;   // clear it
        }
    } else {
        if (dma_hw->ints0 & 1u << pSPI->rx_dma) { // Ours?
            dma_hw->ints0 = 1u << pSPI->rx_dma;   // clear it
        }
    }
}

void set_spi_dma_irq_channel(bool useChannel1, bool shared) {
    irqChannel1 = useChannel1;
    irqShared = shared;
}

bool spi_transfer(spi_t* pSPI, const uint8_t* tx, uint8_t* rx, size_t length) {
    if (tx)
        channel_config_set_read_increment(&pSPI->tx_dma_cfg, true);
    else {
        static const uint8_t dummy = SPI_FILL_CHAR;
        tx = &dummy;
        channel_config_set_read_increment(&pSPI->tx_dma_cfg, false);
    }
    if (rx)
        channel_config_set_write_increment(&pSPI->rx_dma_cfg, true);
    else {
        static uint8_t dummy = 0xA5;
        rx = &dummy;
        channel_config_set_write_increment(&pSPI->rx_dma_cfg, false);
    }
    dma_hw->ints0 = 1u << pSPI->rx_dma;
    dma_channel_configure(pSPI->tx_dma, &pSPI->tx_dma_cfg,
                          &spi_get_hw(pSPI->hw_inst)->dr, // write address
                          tx,                             // read address
                          length,                         // element count (each element is of
                                                          // size transfer_data_size)
                          false);                         // start
    dma_channel_configure(pSPI->rx_dma, &pSPI->rx_dma_cfg,
                          rx,                             // write address
                          &spi_get_hw(pSPI->hw_inst)->dr, // read address
                          length,                         // element count (each element is of
                                                          // size transfer_data_size)
                          false);                         // start
    dma_start_channel_mask((1u << pSPI->tx_dma) | (1u << pSPI->rx_dma));

    /* Timeout 1 sec */
    uint32_t timeOut = 1000;
    /* Wait until master completes transfer or time out has occured. */
    bool rc = 1; /*TODO*/
    if (!rc) {
        // If the timeout is reached the function will return false
        return false;
    }
    // Shouldn't be necessary:
    dma_channel_wait_for_finish_blocking(pSPI->tx_dma);
    dma_channel_wait_for_finish_blocking(pSPI->rx_dma);
    return true;
}

bool pico_spi_init(spi_t* pSPI) {
    if (!pSPI->initialized) {
        spi_init(pSPI->hw_inst, 100 * 1000);
        spi_set_format(pSPI->hw_inst, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
        gpio_set_function(pSPI->miso_gpio, GPIO_FUNC_SPI);
        gpio_set_function(pSPI->mosi_gpio, GPIO_FUNC_SPI);
        gpio_set_function(pSPI->sck_gpio, GPIO_FUNC_SPI);
        if (pSPI->set_drive_strength) {
            gpio_set_drive_strength(pSPI->mosi_gpio, pSPI->mosi_gpio_drive_strength);
            gpio_set_drive_strength(pSPI->sck_gpio, pSPI->sck_gpio_drive_strength);
        }
        gpio_pull_up(pSPI->miso_gpio);
        pSPI->tx_dma = dma_claim_unused_channel(true);
        pSPI->rx_dma = dma_claim_unused_channel(true);
        pSPI->tx_dma_cfg = dma_channel_get_default_config(pSPI->tx_dma);
        pSPI->rx_dma_cfg = dma_channel_get_default_config(pSPI->rx_dma);
        channel_config_set_transfer_data_size(&pSPI->tx_dma_cfg, DMA_SIZE_8);
        channel_config_set_transfer_data_size(&pSPI->rx_dma_cfg, DMA_SIZE_8);
        channel_config_set_dreq(&pSPI->tx_dma_cfg,
                                spi_get_index(pSPI->hw_inst) ? DREQ_SPI1_TX : DREQ_SPI0_TX);
        channel_config_set_write_increment(&pSPI->tx_dma_cfg, false);
        channel_config_set_dreq(&pSPI->rx_dma_cfg,
                                spi_get_index(pSPI->hw_inst) ? DREQ_SPI1_RX : DREQ_SPI0_RX);
        channel_config_set_read_increment(&pSPI->rx_dma_cfg, false);
        int irq = irqChannel1 ? DMA_IRQ_1 : DMA_IRQ_0;
        if (irqShared)
            irq_add_shared_handler(irq, pSPI->dma_isr,
                                   PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);
        else
            irq_set_exclusive_handler(irq, pSPI->dma_isr);
        if (irqChannel1)
            dma_channel_set_irq1_enabled(pSPI->rx_dma, true);
        else
            dma_channel_set_irq0_enabled(pSPI->rx_dma, true);
        irq_set_enabled(irq, true);
        pSPI->initialized = true;
    }
    return true;
}
