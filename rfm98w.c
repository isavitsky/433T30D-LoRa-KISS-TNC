#include "config.h"
#include "rfm98w.h"
#include "spi.h"
#include "uart.h"

void rf_setopmode(uint8_t opmode)
{
  spi_writereg(RF98_REG_01_OP_MODE, RF98_MODE_STDBY);
  spi_writereg(RF98_REG_01_OP_MODE, opmode);
}

void rf_setfreq(uint32_t freq)
{
  rf_setopmode(RF98_MODE_STDBY);
  spi_writereg(RF98_REG_06_FRF_MSB, (uint8_t)(freq >> 16));
  spi_writereg(RF98_REG_07_FRF_MID, (uint8_t)(freq >> 8));
  spi_writereg(RF98_REG_08_FRF_LSB, (uint8_t)(freq));
}

uint32_t rf_getfreq()
{
  uint32_t frq;

  rf_setopmode(RF98_MODE_STDBY);
  frq = (uint32_t)spi_readreg(RF98_REG_06_FRF_MSB) << 16;
  frq |= (uint32_t)spi_readreg(RF98_REG_07_FRF_MID) << 8;
  frq |= spi_readreg(RF98_REG_08_FRF_LSB);

  return frq;
}

void rf_send(char *data, uint8_t len)
{
  rf_setopmode(RF98_MODE_STDBY);
  spi_writereg(RF98_REG_12_IRQ_FLAGS, RF98_TX_DONE); /* clear interrupt */
  //spi_writereg(RF98_REG_12_IRQ_FLAGS, 0xFF); /* clear interrupt */
  spi_writereg(RF98_REG_0E_FIFO_TX_BASE_ADDR, RF_FIFO_TXBASE);
  spi_writereg(RF98_REG_0D_FIFO_ADDR_PTR, RF_FIFO_TXBASE);
  if ( flag.header_implicit )
    spi_writereg(RF98_REG_22_PAYLOAD_LENGTH, 0xFF);
  else
    spi_writereg(RF98_REG_22_PAYLOAD_LENGTH, len);
  spi_writeburst(RF98_REG_00_FIFO, data, len);
  TX_ON;
  rf_setopmode(RF98_MODE_TX);
  while ( ! (spi_readreg(RF98_REG_12_IRQ_FLAGS) & RF98_TX_DONE) ); // block
}

uint8_t rf_recv(void)
{
  uint8_t i, num = 0;

  if ( !(spi_readreg(RF98_REG_12_IRQ_FLAGS) &
        RF98_PAYLOAD_CRC_ERROR) )
  {
    rf_setopmode(RF98_MODE_STDBY);
    spi_writereg(RF98_REG_0D_FIFO_ADDR_PTR, spi_readreg(RF98_REG_10_FIFO_RX_CURRENT_ADDR));
    
    /* In implicit mode 1st byte is packet length */
    if ( flag.header_implicit )
    {
      num = spi_readreg(RF98_REG_00_FIFO);
    } else
    {
      num = spi_readreg(RF98_REG_13_RX_NB_BYTES);
    }

    for (i=0; i<num;i++)
    {
      rf_rxbuf[i] = (char)spi_readreg(RF98_REG_00_FIFO);
    }
    // reset FIFO pointer to RXBASE
    spi_writereg(RF98_REG_0D_FIFO_ADDR_PTR, RF_FIFO_RXBASE);
  }

  spi_writereg(RF98_REG_12_IRQ_FLAGS, RF98_RX_DONE |
               RF98_PAYLOAD_CRC_ERROR | RF98_VALID_HEADER);
  
  rf_setopmode(RF98_MODE_RXCONTINUOUS);
  while ( spi_readreg(RF98_REG_01_OP_MODE) != (RF98_LONG_RANGE_MODE | RF98_MODE_RXCONTINUOUS) );

  return num;
}
