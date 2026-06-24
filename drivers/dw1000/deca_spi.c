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
#include <zephyr/pm/device_runtime.h>

#include <nrfx.h>
#include <nrfx_spim.h>

LOG_MODULE_REGISTER(deca_spi, LOG_LEVEL_INF);

// hack to improve SPI speed
// first two fields (diving into spi_context) of struct at
// https://github.com/zephyrproject-rtos/zephyr/blob/684c9e8f32e4373a21098559f748f06915f950c9/drivers/spi/spi_nrfx_spim.c#L36
struct spi_nrfx_data {
	nrfx_spim_t spim;
	struct spi_config* spi_cfg;
};

// rewritten version of
// https://github.com/zephyrproject-rtos/zephyr/blob/684c9e8f32e4373a21098559f748f06915f950c9/drivers/spi/spi_nrfx_spim.c#L364
// no overhead related to SPI context support (mostly, spinlock instead of semaphor blocking take every RX/TX buffer)
// configuration is still performed by Zephyr by making a single original spi_transceive call to avoid excessive duplicate code
static int spi_transceive_fast(const struct device *dev,
		      const struct spi_config *spi_cfg,
		      const struct spi_buf_set *tx_bufs,
		      const struct spi_buf_set *rx_bufs)
{
	struct spi_nrfx_data *dev_data = dev->data;
	int ret;

    // if the configration was updated (or not configured at all), use ordinary spi_transcieve
    if ((dev_data->spi_cfg->operation != spi_cfg->operation) ||
        (dev_data->spi_cfg->frequency != spi_cfg->frequency) ||
        (dev_data->spi_cfg->word_delay != spi_cfg->word_delay))
    {
        return spi_transceive(dev, spi_cfg, tx_bufs, rx_bufs);
    }

	ret = pm_device_runtime_get(dev);
	if (ret) {
		return ret;
	}

	if (spi_cfg->cs.cs_is_gpio) {
		gpio_pin_set_dt(&spi_cfg->cs.gpio, 1);
	}
    
    int maxcnt = 254;
    int buf_i = 0;
    int buf_off = 0;
    int non_first = 0;

    while (true)
    {
        nrfx_spim_xfer_desc_t xfer;
        size_t delta = 0;
        int next_buff = 0;
        xfer.tx_length = 0;
        xfer.rx_length = 0;
        if (tx_bufs && (buf_i < tx_bufs->count))
        {
            xfer.p_tx_buffer = tx_bufs->buffers[buf_i].buf;
            if (xfer.p_tx_buffer)
            {
                xfer.p_tx_buffer += buf_off;
                if (tx_bufs->buffers[buf_i].len - buf_off < maxcnt)
                {
                    xfer.tx_length = tx_bufs->buffers[buf_i].len - buf_off;
                    next_buff = 1;
                }
                else
                {
                    xfer.tx_length = maxcnt;
                    delta = maxcnt;
                }
            }
        }
        else
        {
            xfer.p_tx_buffer = 0;
        }
        if (rx_bufs && (buf_i < rx_bufs->count))
        {
            xfer.p_rx_buffer = rx_bufs->buffers[buf_i].buf;
            if (xfer.p_rx_buffer)
            {
                xfer.p_rx_buffer += buf_off;
                if (rx_bufs->buffers[buf_i].len - buf_off < maxcnt)
                {
                    xfer.rx_length = rx_bufs->buffers[buf_i].len - buf_off;
                    next_buff = 1;
                }
                else
                {
                    xfer.rx_length = maxcnt;
                    delta = maxcnt;
                }
            }
        }
        else
        {
            xfer.p_rx_buffer = 0;
        }
        if (!delta && !next_buff)
            break;
        
        if (non_first)
            while (!nrf_spim_event_check(dev_data->spim.p_reg, NRF_SPIM_EVENT_END));
        nrf_spim_event_clear(dev_data->spim.p_reg, NRF_SPIM_EVENT_END);
        nrfx_spim_xfer(&dev_data->spim, &xfer, NRFX_SPIM_FLAG_NO_XFER_EVT_HANDLER);
        non_first = 1;

        if (next_buff)
        {
            buf_off = 0;
            buf_i++;
        }
        else
        {
            buf_off += delta;
        }

    }

    if (non_first)
        while (!nrf_spim_event_check(dev_data->spim.p_reg, NRF_SPIM_EVENT_END))
        {}

	if (spi_cfg->cs.cs_is_gpio) {
	    gpio_pin_set_dt(&spi_cfg->cs.gpio, 0);
    }

	pm_device_runtime_put(dev);
	return ret;
}


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
    // prevent SPI from sleeping between transactions, as waking it up every time takes time
    pm_device_runtime_get(spi_dev.bus);
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

    int ret = spi_transceive_fast(spi_dev.bus, spi_cfg, &tx, &rx);

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

    int ret = spi_transceive_fast(spi_dev.bus, spi_cfg, &tx, &rx);

#if 0
    LOG_HEXDUMP_INF(headerBuffer, headerLength, "readfromspi: Header");
    LOG_HEXDUMP_INF(readBuffer, readLength, "readfromspi: Body");
#endif

    return ret;
}
