#include "config.h"
#include "bridge.h"
#include "spi.h"
#include "rfm98w.h"
#include "uart.h"
#include "timer.h"

#define IRQ_CHECK()             ( spi_readreg(RF98_REG_12_IRQ_FLAGS) & \
                            (RF98_RX_DONE | RF98_VALID_HEADER) )

#define MODEMSTAT_CHECK()       ( spi_readreg(RF98_REG_18_MODEM_STAT) & \
                            RF98_MODEM_STATUS_SIGNAL_DETECTED )

static void slice_and_transmit(void);

void bridge()
{
  uint8_t len;

  tim1_start();
  tim2_start();
  CBUF_Init(urxcbuf);
  uart_stop();
  uart_init(cfg[CFG_TNC_IDX] & CFG_TNC_SSP_MASK);
  RTS_SET_LOW;

  rf_setopmode(RF98_MODE_STDBY);
  spi_writereg(RF98_REG_12_IRQ_FLAGS, 0xFF); // clear interrupts
  spi_writereg(RF98_REG_0F_FIFO_RX_BASE_ADDR, RF_FIFO_RXBASE);

  /* Set header implicit flag */
  if ( spi_readreg(RF98_REG_1D_MODEM_CONFIG1) &
   RF98_IMPLICIT_HEADER_MODE_ON )
  {
    flag.header_implicit = true;
    /* max PayloadLength for receiving */
    spi_writereg(RF98_REG_22_PAYLOAD_LENGTH, 0xFF);
  } else
  {
    flag.header_implicit = false;
  }
  
  rf_setopmode(RF98_MODE_RXCONTINUOUS);
  while ( spi_readreg(RF98_REG_01_OP_MODE) != (RF98_LONG_RANGE_MODE | RF98_MODE_RXCONTINUOUS) );
  RX_ON;
  
  do
  {
    /* process incoming Radio data */
    if ( spi_readreg(RF98_REG_12_IRQ_FLAGS) & RF98_RX_DONE )
    {
      tim1_restart();
      len = rf_recv();
      for (uint8_t i = 0; i < len; i++)
      {
        uart_putchar(rf_rxbuf[i]);
      }
    }

    /* process incoming UART data */
    while ( CBUF_Len(utmpbuf) && !flag.uart_rx_complete )
    {
      cbuf_push(cbuf_pop2());
    }

    if ( flag.uart_rx_complete )
    {
      tim1_restart();
      if ( !IRQ_CHECK() && !MODEMSTAT_CHECK() ) // transmit if not receiving
      {
        slice_and_transmit();
      }
    }
    adaptive_sleep();
  } while (flag.tnc_mode == MODE_BRIDGE);
}

static void slice_and_transmit()
{
  uint16_t len;
  uint8_t payload_sz = RF96_MTU;

  len = CBUF_Len(urxcbuf);
  if ( flag.header_implicit )
    payload_sz--;
  if ( len )
  {
    while ( len > payload_sz )
    {
      len -= payload_sz;
      rf_send(rf_txbuf, assemble(payload_sz, 0));
      while ( ! (spi_readreg(RF98_REG_12_IRQ_FLAGS) & RF98_TX_DONE) ); // block
    }

    if ( len > 0 )
    {
      rf_send(rf_txbuf, assemble(len, 0));
      while ( ! (spi_readreg(RF98_REG_12_IRQ_FLAGS) & RF98_TX_DONE) ); // block
    }

    /* Return to RX */
    RX_ON;
    //rf_setopmode(RF98_MODE_STDBY); /* <-- after TX already in STDBY */
    spi_writereg(RF98_REG_12_IRQ_FLAGS, RF98_TX_DONE); /* clear interrupt */
    /* set maximum PayloadLength for receiving in implicit mode */
    if ( flag.header_implicit )
    {
      spi_writereg(RF98_REG_22_PAYLOAD_LENGTH, 0xFF);
    }
    rf_setopmode(RF98_MODE_RXCONTINUOUS);
    while ( spi_readreg(RF98_REG_01_OP_MODE) != (RF98_LONG_RANGE_MODE | RF98_MODE_RXCONTINUOUS) );
  }

  flag.uart_rx_complete = false;
}
