#include "sd_spi.h"
#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "pico/stdlib.h"

#define SD_CS_PIN 22
#define SD_CLK_PIN VGABOARD_SD_CLK_PIN
#define SD_MOSI_PIN VGABOARD_SD_CMD_PIN
#define SD_MISO_PIN VGABOARD_SD_DAT0_PIN

static PIO pio_sd = pio1;
static int sd_sm = -1;
static int dma_tx = -1;
static int dma_rx = -1;
SD_TYPE sd_type = sdtpUnk;

#define sd_spi_wrap_target 0
#define sd_spi_wrap 1

static const uint16_t sd_spi_program_instructions[] = {
            //     .wrap_target
    0x6301, //  0: out    pins, 1         side 0 [3]
    0x5301, //  1: in     pins, 1         side 1 [3]
            //     .wrap
};

static const struct pio_program sd_spi_program = {
    .instructions = sd_spi_program_instructions,
    .length = sizeof(sd_spi_program_instructions) / sizeof(uint16_t),
    .origin = -1,
};

bool sd_spi_load(void) {
    dma_tx = dma_claim_unused_channel(true);
    dma_rx = dma_claim_unused_channel(true);
    if ((dma_tx < 0) || (dma_rx < 0))
        return false;
    gpio_init(SD_CS_PIN);
    gpio_set_dir(SD_CS_PIN, GPIO_OUT);
    gpio_pull_up(SD_MISO_PIN);
    gpio_put(SD_CS_PIN, 1);
    uint offset = pio_add_program(pio_sd, &sd_spi_program);
    sd_sm = pio_claim_unused_sm(pio_sd, true);
    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset + sd_spi_wrap_target, offset + sd_spi_wrap);
    sm_config_set_sideset(&c, 1, false, false);
    sm_config_set_out_pins(&c, SD_MOSI_PIN, 1);
    sm_config_set_in_pins(&c, SD_MISO_PIN);
    sm_config_set_sideset_pins(&c, SD_CLK_PIN);
    sm_config_set_out_shift(&c, false, true, 8);
    sm_config_set_in_shift(&c, false, true, 8);
    pio_sm_set_pins_with_mask(pio_sd, sd_sm, 0, (1 << SD_CLK_PIN) | (1 << SD_MOSI_PIN));
    pio_sm_set_pindirs_with_mask(pio_sd, sd_sm, (1 << SD_CLK_PIN) | (1 << SD_MOSI_PIN),
                                 (1 << SD_CLK_PIN) | (1 << SD_MOSI_PIN) | (1 << SD_MISO_PIN));
    pio_gpio_init(pio_sd, SD_CLK_PIN);
    pio_gpio_init(pio_sd, SD_MOSI_PIN);
    pio_gpio_init(pio_sd, SD_MISO_PIN);
    pio_sm_init(pio_sd, sd_sm, offset, &c);
    pio_sm_set_enabled(pio_sd, sd_sm, true);
    return true;
}

void sd_spi_unload(void) {
    pio_sm_set_enabled(pio_sd, sd_sm, false);
    pio_sm_unclaim(pio_sd, sd_sm);
    dma_channel_unclaim(dma_tx);
    dma_channel_unclaim(dma_rx);
    sd_sm = -1;
    dma_tx = -1;
    dma_rx = -1;
}

void sd_spi_freq(float freq) {
    float div = clock_get_hz(clk_sys) / (4000.0 * freq);
    pio_sm_set_clkdiv(pio_sd, sd_sm, div);
}

void sd_spi_chpsel(bool sel) { gpio_put(SD_CS_PIN, !sel); }

// Do 8 bit accesses on FIFO, so that write data is byte-replicated. This
// gets us the left-justification for free (for MSB-first shift-out)
void sd_spi_xfer(bool bWrite, const uint8_t* src, uint8_t* dst, size_t len) {
    io_rw_8* txfifo = (io_rw_8*)&pio_sd->txf[sd_sm];
    io_rw_8* rxfifo = (io_rw_8*)&pio_sd->rxf[sd_sm];
    dma_hw->sniff_data = 0;
    dma_channel_config c = dma_channel_get_default_config(dma_rx);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_8);
    channel_config_set_enable(&c, true);
    channel_config_set_read_increment(&c, false);
    channel_config_set_write_increment(&c, !bWrite);
    channel_config_set_dreq(&c, DREQ_PIO1_RX0 + sd_sm);
    if (!bWrite) {
        channel_config_set_sniff_enable(&c, true);
        dma_sniffer_enable(dma_rx, DMA_SNIFF_CTRL_CALC_VALUE_CRC16, true);
    }
    dma_channel_configure(dma_rx, &c, dst, rxfifo, len, true);
    c = dma_channel_get_default_config(dma_tx);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_8);
    channel_config_set_enable(&c, true);
    channel_config_set_read_increment(&c, bWrite);
    channel_config_set_write_increment(&c, false);
    channel_config_set_dreq(&c, DREQ_PIO1_TX0 + sd_sm);
    if (bWrite) {
        channel_config_set_sniff_enable(&c, true);
        dma_sniffer_enable(dma_tx, DMA_SNIFF_CTRL_CALC_VALUE_CRC16, true);
    }
    dma_channel_configure(dma_tx, &c, txfifo, src, len, true);
    dma_channel_wait_for_finish_blocking(dma_rx);
}

uint8_t sd_spi_put(const uint8_t* src, size_t len) {
    uint8_t resp;
    sd_spi_xfer(true, src, &resp, len);
    return resp;
}

void sd_spi_get(uint8_t* dst, size_t len) {
    uint8_t fill = 0xFF;
    sd_spi_xfer(false, &fill, dst, len);
}

uint8_t sd_spi_clk(size_t len) {
    size_t tx_remain = len;
    size_t rx_remain = len;
    uint8_t resp;
    io_rw_8* txfifo = (io_rw_8*)&pio_sd->txf[sd_sm];
    io_rw_8* rxfifo = (io_rw_8*)&pio_sd->rxf[sd_sm];
    while (tx_remain || rx_remain) {
        if (tx_remain && !pio_sm_is_tx_fifo_full(pio_sd, sd_sm)) {
            *txfifo = 0xFF;
            --tx_remain;
        }
        if (rx_remain && !pio_sm_is_rx_fifo_empty(pio_sd, sd_sm)) {
            resp = *rxfifo;
            --rx_remain;
        }
    }
    return resp;
}

#define SD_R1_OK 0x00
#define SD_R1_IDLE 0x01
#define SD_R1_ILLEGAL 0x04

#define SDBT_START 0xFE  // Start of data token
#define SDBT_ERRMSK 0xF0 // Mask to select zero bits in error token
#define SDBT_ERANGE 0x08 // Out of range error flag
#define SDBT_EECC 0x04   // Card ECC failed
#define SDBT_ECC 0x02    // CC error
#define SDBT_ERROR 0x01  // Error
#define SDBT_ECLIP 0x10  // Value above all error bits

static uint8_t cmd0[] = {0xFF, 0x40 | 0, 0x00, 0x00, 0x00, 0x00, 0x95};   // Go Idle
static uint8_t cmd8[] = {0xFF, 0x40 | 8, 0x00, 0x00, 0x01, 0xAA, 0x87};   // Set interface condition
static uint8_t cmd9[] = {0xFF, 0x40 | 9, 0x00, 0x00, 0x00, 0xad, 0x00};   // Read CSD
static uint8_t cmd17[] = {0xFF, 0x40 | 17, 0x00, 0x00, 0x00, 0x00, 0x00}; // Read single block
static uint8_t cmd24[] = {0xFF, 0x40 | 24, 0x00, 0x00, 0x00, 0x00, 0x00}; // Write single block
static uint8_t cmd55[] = {0xFF, 0x40 | 55, 0x00, 0x00,
                          0x01, 0xAA,      0x65}; // Application command follows
static uint8_t cmd58[] = {0xFF, 0x40 | 58, 0x00, 0x00,
                          0x00, 0x00,      0xFD}; // Read Operating Condition Reg.
static uint8_t acmd41[] = {0xFF, 0x40 | 41, 0x40, 0x00,
                           0x00, 0x00,      0x77}; // Set operation condition

uint8_t sd_spi_cmd(uint8_t* src) {

    uint8_t resp = sd_spi_put(src, 7);
    for (int i = 0; i < 100; ++i) {
        if (!(resp & 0x80))
            break;
        resp = sd_spi_clk(1);
    }
    return resp;
#if 0
    uint8_t resp = sd_spi_put (src, 7);
    while ( resp & 0x80 )
        {
        resp = sd_spi_clk (1);
        }
    return resp;
#endif
}

bool sd_spi_init(void) {
    uint8_t chk[4];
    uint8_t resp;
    if (sd_sm < 0)
        sd_spi_load();
    sd_type = sdtpUnk;
    sd_spi_freq(200);
    sd_spi_chpsel(false);
    sd_spi_clk(10);
    for (int i = 0; i < 256; ++i) {
        sd_spi_chpsel(true);
        resp = sd_spi_cmd(cmd0);
        if (resp == SD_R1_IDLE)
            break;
        sd_spi_chpsel(false);
        sleep_ms(1);
    }
    if (resp != SD_R1_IDLE) {
        return false;
    }
    for (int i = 0; i < 10; ++i) {
        resp = sd_spi_cmd(cmd8);
        if (resp == SD_R1_IDLE) {
            sd_spi_get(chk, 4);
            if (chk[3] == cmd8[4])
                break;
        } else if (resp & SD_R1_ILLEGAL) {
            break;
        }
    }
    if (resp & SD_R1_ILLEGAL) {
        sd_type = sdtpVer1;
    } else if (resp != SD_R1_IDLE) {
        sd_spi_chpsel(false);
        return false;
    }
    for (int i = 0; i < 256; ++i) {
        resp = sd_spi_cmd(cmd55);
        resp = sd_spi_cmd(acmd41);
        if (resp == SD_R1_OK)
            break;
    }
    if (resp != SD_R1_OK) {
        sd_spi_chpsel(false);
        return false;
    }
    if (sd_type == sdtpUnk) {
        resp = sd_spi_cmd(cmd58);
        if (resp != SD_R1_OK) {
            sd_spi_chpsel(false);
            return false;
        }
        sd_spi_get(chk, 4);
        if (chk[0] & 0x40) {
            sd_type = sdtpHigh;
        } else {
            sd_type = sdtpVer2;
        }
    }
    sd_spi_freq(25000);
    return true;
}

void sd_spi_term(void) {
    sd_type = sdtpUnk;
    sd_spi_chpsel(false);
    sd_spi_freq(200);
}

void sd_spi_set_crc7(uint8_t* pcmd) {
    uint8_t crc = 0;
    for (int i = 0; i < 5; ++i) {
        uint8_t v = *pcmd;
        for (int j = 0; j < 8; ++j) {
            if ((v ^ crc) & 0x80)
                crc ^= 0x09;
            crc <<= 1;
            v <<= 1;
        }
        ++pcmd;
    }
    *pcmd = crc + 1;
}

void sd_spi_set_lba(uint lba, uint8_t* pcmd) {
    if (sd_type != sdtpHigh)
        lba <<= 9;
    pcmd += 5;
    for (int i = 0; i < 4; ++i) {
        *pcmd = lba & 0xFF;
        lba >>= 8;
        --pcmd;
    }
    sd_spi_set_crc7(pcmd);
}

bool sd_spi_read(uint32_t lba, uint8_t* buff) {
    uint8_t chk[2];
    sd_spi_set_lba(lba, cmd17);
    uint8_t resp = sd_spi_cmd(cmd17);
    if (resp != SD_R1_OK) {
        return false;
    }
    while (true) {
        resp = sd_spi_clk(1);
        if (resp == SDBT_START)
            break;
        if (resp < SDBT_ECLIP) {
            return false;
        }
    }
    sd_spi_get(buff, 512);
    uint16_t crc = dma_hw->sniff_data;
    sd_spi_get(chk, 2);
    if ((chk[0] != (crc >> 8)) || (chk[1] != (crc & 0xFF))) {
        return false;
    }
    return true;
}

bool sd_spi_write(uint32_t lba, const uint8_t* buff) {
    uint8_t chk[2];
    sd_spi_set_lba(lba, cmd24);
    uint8_t resp = sd_spi_cmd(cmd24);
    if (resp != SD_R1_OK) {
        return false;
    }
    resp = SDBT_START;
    resp = sd_spi_put(&resp, 1);
    resp = sd_spi_put(buff, 512);
    uint16_t crc = dma_hw->sniff_data;
    chk[0] = crc >> 8;
    chk[1] = crc & 0xFF;
    sd_spi_put(chk, 2);
    bool bResp = false;
    while (true) {
        resp = sd_spi_clk(1);
        switch (resp & 0x0E) {
        case 0x04:
            bResp = true;
            break;
        case 0xA0:
            bResp = false;
            break;
        case 0xC0:
            bResp = false;
            break;
        default:
            break;
        }
        if (resp == 0xFF)
            break;
    }
    return bResp;
}

int sd_spi_sectors(void) {
    unsigned char csd[16];
    uint8_t chk[2];
    uint8_t resp = sd_spi_cmd(cmd9);
    if (resp != SD_R1_OK) {
        return 0;
    }
    while (true) {
        resp = sd_spi_clk(1);
        if (resp == SDBT_START)
            break;
        if (resp < SDBT_ECLIP) {
            return false;
        }
    }
    sd_spi_get(csd, 16);
    uint16_t crc = dma_hw->sniff_data;
    sd_spi_get(chk, 2);
    if ((chk[0] != (crc >> 8)) || (chk[1] != (crc & 0xFF))) {
        return 0;
    }
    int cap;
    unsigned short csize;
    if ((csd[0] & 0xC0) == 0x40) { // V2.00 card
        csize = csd[9] + ((unsigned short)csd[8] << 8) + 1;
        cap = (unsigned short)csize << 10; // Get the number of sectors
    } else {                               // V1.XX card
        unsigned char n = (csd[5] & 15) + ((csd[10] & 128) >> 7) + ((csd[9] & 3) << 1) + 2;
        csize = (csd[8] >> 6) + ((unsigned short)csd[7] << 2) +
                ((unsigned short)(csd[6] & 3) << 10) + 1;
        cap = (unsigned short)csize << (n - 9); // Get the number of sectors
    }
    return cap;
}
