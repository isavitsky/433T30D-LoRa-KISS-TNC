#include "config.h"
#include "spi.h"

void spi_init()
{
          // SPI
        CLK_PCKENR1_PCKEN14 = 1;       // clock
        SPI1_CR2 |= (MASK_SPI1_CR2_SSM | MASK_SPI1_CR2_SSI); // soft mode + master select
        SPI1_CR1 |= (MASK_SPI1_CR1_SPE | MASK_SPI1_CR1_MSTR); // master mode + SPI enable
}

void spi_deinit()
{
  CLK_PCKENR1_PCKEN14 = 0;
}

static uint8_t spi_sendbyte(uint8_t data)
{
  WAIT_SPI1_TXNE;
  SPI1_DR = data;
  WAIT_SPI1_RXE;
  return SPI1_DR;
}

uint8_t spi_readreg(uint8_t reg)
{
  uint8_t val;

  CS_HIGH;
  spi_sendbyte(reg & 0x7F);
  val = spi_sendbyte(0);
  CS_LOW;
  return val;
}

uint8_t spi_writereg(uint8_t reg, uint8_t byte)
{
  uint8_t val;

  CS_HIGH;
  spi_sendbyte(reg | 0x80);
  val = spi_sendbyte(byte);
  CS_LOW;
  return val;
}

void spi_writeburst(uint8_t reg, char *data, uint8_t len)
{
  CS_HIGH;
  spi_sendbyte(reg | 0x80);
  while(len--) spi_sendbyte(*data++);
  CS_LOW;
}
