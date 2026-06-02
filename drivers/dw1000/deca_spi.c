/*! ----------------------------------------------------------------------------
 * @file    deca_spi.c
 * @brief   SPI access functions
 *
 * @copyright
 * Copyright 2015 (c) DecaWave Ltd, Dublin, Ireland.
 * Copyright 2019 (c) Frederic Mes, RTLOC.
 * Copyright 2019 (c) Callender-Consulting LLC.
 *
 * All rights reserved.
 *
 * @author DecaWave
 */

#include "deca_spi.h"

#include <zephyr/device.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>

LOG_MODULE_REGISTER(deca_spi, LOG_LEVEL_INF);

static const struct spi_dt_spec spi_dev = SPI_DT_SPEC_GET(DT_NODELABEL(dw1000), SPI_WORD_SET(8) | SPI_TRANSFER_MSB);

const struct spi_config *spi_cfg;
static struct spi_config spi_cfg_slow;

int dw1000_spi_init()
{
    int ret = spi_is_ready_dt(&spi_dev);
    if(!ret)
    {
        LOG_ERR("SPI device not ready");
        return -ENODEV;
    }
    memcpy(&spi_cfg_slow, &spi_dev.config, sizeof(spi_cfg_slow));
    dw1000_spi_speed_slow();

    return 0;
}

void dw1000_spi_speed_slow()
{
    spi_cfg_slow.frequency = 2000000;
    spi_cfg = &spi_cfg_slow;
}

void dw1000_spi_speed_fast()
{
    spi_cfg = &spi_dev.config;
}

/*
 * Function: writetospi()
 *
 * Low level abstract function to write to the SPI
 * Takes two separate byte buffers for write header and write data
 * returns 0 for success
 */
int writetospi(uint16_t headerLength, const uint8_t *headerBuffer, uint32_t bodyLength,
               const uint8_t *bodyBuffer)
{
#if 0
    LOG_HEXDUMP_INF(headerBuffer, headerLength, "writetospi: Header");
    LOG_HEXDUMP_INF(bodyBuffer, bodyLength, "writetospi: Body");
#endif

    const struct spi_buf tx_bufs[2] = {
        {
            .buf = (uint8_t *)headerBuffer,
            .len = headerLength
        },
        {
            .buf = (uint8_t *)bodyBuffer,
            .len = bodyLength
        }
    };
    const struct spi_buf_set tx = {
        .buffers = tx_bufs,
        .count = 2
    };

    const struct spi_buf_set rx = {
        .buffers = NULL,
        .count = 0
    };

    int ret = spi_transceive(spi_dev.bus, spi_cfg, &tx, &rx);

    return ret;
}

/*
 * Function: readfromspi()
 *
 * Low level abstract function to read from the SPI
 * Takes two separate byte buffers for write header and read data
 * returns the offset into read buffer where first byte of read data
 * may be found, or returns 0
 */
int readfromspi(uint16_t headerLength, const uint8_t *headerBuffer, uint32_t readLength,
                uint8_t *readBuffer)
{
    const struct spi_buf tx_bufs = {
        .buf = (uint8_t *)headerBuffer,
        .len = headerLength
    };
    const struct spi_buf_set tx = {
        .buffers = &tx_bufs,
        .count = 1
    };

    struct spi_buf rx_bufs[2] = {
        {
            .buf = NULL,
            .len = headerLength
        },
        {
            .buf = readBuffer,
            .len = readLength
        }
    };
    const struct spi_buf_set rx = {
        .buffers = rx_bufs,
        .count = 2
    };

    int ret = spi_transceive(spi_dev.bus, spi_cfg, &tx, &rx);

#if 0
    LOG_HEXDUMP_INF(headerBuffer, headerLength, "readfromspi: Header");
    LOG_HEXDUMP_INF(readBuffer, readLength, "readfromspi: Body");
#endif

    return ret;
}
