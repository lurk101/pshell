/* sd_spi.c
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

/* Standard includes. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
//
#include "hardware/gpio.h"
//
#include "sd_card.h"
#include "sd_spi.h"
#include "spi.h"

void sd_spi_go_high_frequency(sd_card_t* pSD) {
    uint actual = spi_set_baudrate(pSD->spi->hw_inst, pSD->spi->baud_rate);
}
void sd_spi_go_low_frequency(sd_card_t* pSD) {
    uint actual = spi_set_baudrate(pSD->spi->hw_inst, 400 * 1000); // Actual frequency: 398089
}

static void sd_spi_select(sd_card_t* pSD) {
    gpio_put(pSD->ss_gpio, 0);
    // A fill byte seems to be necessary, sometimes:
    uint8_t fill = SPI_FILL_CHAR;
    spi_write_blocking(pSD->spi->hw_inst, &fill, 1);
}

static void sd_spi_deselect(sd_card_t* pSD) {
    gpio_put(pSD->ss_gpio, 1);
    uint8_t fill = SPI_FILL_CHAR;
    spi_write_blocking(pSD->spi->hw_inst, &fill, 1);
}

void sd_spi_deselect_pulse(sd_card_t* pSD) {
    sd_spi_deselect(pSD);
    // tCSH Pulse duration, CS high 200 ns
    sd_spi_select(pSD);
}

void sd_spi_acquire(sd_card_t* pSD) {
    sd_spi_select(pSD);
}

void sd_spi_release(sd_card_t* pSD) {
    sd_spi_deselect(pSD);
}

bool sd_spi_transfer(sd_card_t* pSD, const uint8_t* tx, uint8_t* rx, size_t length) {
    return spi_transfer(pSD->spi, tx, rx, length);
}

uint8_t sd_spi_write(sd_card_t* pSD, const uint8_t value) {
    uint8_t received = SPI_FILL_CHAR;
    bool success = spi_transfer(pSD->spi, &value, &received, 1);
    return received;
}

void sd_spi_send_initializing_sequence(sd_card_t* pSD) {
    bool old_ss = gpio_get(pSD->ss_gpio);
    // Set DI and CS high and apply 74 or more clock pulses to SCLK:
    gpio_put(pSD->ss_gpio, 1);
    uint8_t ones[10];
    memset(ones, 0xFF, sizeof ones);
    absolute_time_t timeout_time = make_timeout_time_ms(1);
    do {
        sd_spi_transfer(pSD, ones, NULL, sizeof ones);
    } while (0 < absolute_time_diff_us(get_absolute_time(), timeout_time));
    gpio_put(pSD->ss_gpio, old_ss);
}

void sd_spi_init_pl022(sd_card_t* pSD) {
    gpio_set_function(pSD->ss_gpio, GPIO_FUNC_SPI);
}

