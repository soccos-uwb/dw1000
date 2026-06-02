/*! ----------------------------------------------------------------------------
 * @file	deca_spi.h
 * @brief	SPI access functions
 *
 * @copyright
 * Copyright 2015 (c) DecaWave Ltd, Dublin, Ireland.
 *
 * All rights reserved.
 *
 * @author DecaWave
 */

#ifndef _DECA_SPI_H_
#define _DECA_SPI_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

int dw1000_spi_init();

void dw1000_spi_speed_slow();

void dw1000_spi_speed_fast();

/*! ------------------------------------------------------------------------------------------------------------------
 * @fn writetospi()
 *
 * @brief
 * NB: In porting this to a particular microprocessor, the implementer needs to define the two low
 * level abstract functions to write to and read from the SPI the definitions should be in deca_spi.c file.
 * Low level abstract function to write to the SPI
 * Takes two separate byte buffers for write header and write data
 * returns 0 for success, or -1 for error
 *
 * Note: The body of this function is defined in deca_spi.c and is platform specific
 *
 * input parameters:
 * @param headerLength  - number of bytes header being written
 * @param headerBuffer  - pointer to buffer containing the 'headerLength' bytes of header to be written
 * @param bodylength    - number of bytes data being written
 * @param bodyBuffer    - pointer to buffer containing the 'bodylength' bytes od data to be written
 *
 * output parameters
 *
 * returns DWT_SUCCESS for success, or DWT_ERROR for error
 */
int writetospi(uint16_t headerLength, const uint8_t *headerBuffer, uint32_t bodylength, const uint8_t *bodyBuffer);

/*! ------------------------------------------------------------------------------------------------------------------
 * @fn readfromspi()
 *
 * @brief
 * NB: In porting this to a particular microprocessor, the implementer needs to define the two low
 * level abstract functions to write to and read from the SPI the definitions should be in deca_spi.c file.
 * Low level abstract function to write to the SPI
 * Takes two separate byte buffers for write header and write data
 * returns 0 for success, or -1 for error
 *
 * Note: The body of this function is defined in deca_spi.c and is platform specific
 *
 * input parameters:
 * @param headerLength  - number of bytes header to write
 * @param headerBuffer  - pointer to buffer containing the 'headerLength' bytes of header to write
 * @param readlength    - number of bytes data being read
 * @param readBuffer    - pointer to buffer containing to return the data (NB: size required = headerLength + readlength)
 *
 * output parameters
 *
 * returns DWT_SUCCESS for success (and the position in the buffer at which data begins), or DWT_ERROR for error
 */
int readfromspi(uint16_t headerLength, const uint8_t *headerBuffer, uint32_t readlength, uint8_t *readBuffer);

#ifdef __cplusplus
}
#endif

#endif /* _DECA_SPI_H_ */
