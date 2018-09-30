// spi.h
#ifndef SPI_H
#define SPI_H

#include "config.h"

#define CS_HIGH         PB_ODR_ODR4=LOW
#define CS_LOW          PB_ODR_ODR4=HIGH

#define WAIT_SPI1_TXNE  while(!SPI1_SR_TXE)
#define WAIT_SPI1_RXE   while(!SPI1_SR_RXNE)


//-------------- Declaration of function prototypes -----------------
void spi_init(void);
void spi_deinit(void);
uint8_t spi_readreg(uint8_t);
uint8_t spi_writereg(uint8_t, uint8_t);
void spi_writeburst(uint8_t, char *, uint8_t);

#endif
