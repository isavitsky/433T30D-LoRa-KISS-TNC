#include "config.h"
#include "kiss.h"
#include "spi.h"
#include "rfm98w.h"
#include "uart.h"
#include "timer.h"

// Radio frame first byte:
// 2 - first multiframe
// 1 - intermediate multiframe
// 0 - last multiframe
// other value - single frame


#define IRQ_CHECK()             ( spi_readreg(RF98_REG_12_IRQ_FLAGS) & \
                            (RF98_RX_DONE | RF98_VALID_HEADER) )

#define MODEMSTAT_CHECK()       ( spi_readreg(RF98_REG_18_MODEM_STAT) & \
                            RF98_MODEM_STATUS_SIGNAL_DETECTED )

static uint16_t pkt_len;        // length of packet to transmit via RF
static uint16_t tmp_len;

static void kiss_in(uint8_t);
static void kiss_out(uint8_t);
static void kiss_process_cmd(void);
static void csma_transmit(void);
static void cmd_set_txdelay(void);
static void cmd_set_p(void);
static void cmd_set_slottime(void);

void kiss()
{
  flag.txdelay_engaged = false;
  flag.slottime_engaged = false;
  flag.kiss_start_sent = false;

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
    /* process incoming radio data */
    if ( spi_readreg(RF98_REG_12_IRQ_FLAGS) & RF98_RX_DONE )
    {
      kiss_out(rf_recv());
    }

    /* process incoming UART data */
    while ( CBUF_Len(utmpbuf) && !flag.kiss_frame_ready )
    {
      kiss_in(cbuf_pop2());
    }

    if ( flag.kiss_frame_ready )
    {
      tim1_restart();
      kiss_process_cmd();
    }
    adaptive_sleep();
  } while (flag.tnc_mode == MODE_KISS);
}

static void kiss_in(uint8_t byte)
{
  switch(byte)
  {
  case FEND:
    if ( flag.kiss_payload )
    {
      /* EOF received */
      flag.kiss_sof = false;
      flag.kiss_payload = false;
      flag.kiss_escaped = false;
      flag.kiss_frame_ready = true;
      pkt_len = tmp_len;
    }
    else {
      /* SOF received */
      tmp_len = 0;
      flag.kiss_sof = true;
    }
    break;
  case FESC:
    if ( flag.kiss_payload )
    {
      flag.kiss_escaped = true;
    }
    break;
  default:
    if ( flag.kiss_sof )
    {
      flag.kiss_sof = false;
      flag.kiss_payload = true;
    }
    
    if ( flag.kiss_payload )
    {
      if ( !flag.kiss_escaped )
      {
        cbuf_push(byte);
        tmp_len++;
      }
      else {
        flag.kiss_escaped = false;

        switch ( byte )
        {
        case TFESC:
          cbuf_push(FESC);
          tmp_len++;
          break;
        case TFEND:
          cbuf_push(FEND);
          tmp_len++;
          break;
        default:
          // framing error, drop the frame
          flag.kiss_sof = false;
          flag.kiss_payload = false;
          flag.kiss_escaped = false;
          flag.kiss_frame_ready = false;
        } // switch

      } // else kiss_escaped
    } // if kiss_payload

  } // switch
}

static void kiss_out(const uint8_t len)
{
  uint8_t idx = 0;

  if ( rf_rxbuf[0] >= FRAME_FIRST )
  {
    if ( flag.kiss_start_sent ) // error in framing, start over
      uart_putchar(FEND);

    uart_putchar(FEND);
    uart_putchar(CMD_DATA_FRAME);
    flag.kiss_start_sent = true;
  }

  /* check for multiframe */
  if ( rf_rxbuf[0] <= FRAME_FIRST )
    idx++;
  
  for (uint8_t i = idx; i < len; i++)
  {
    switch ( rf_rxbuf[i] )
    {
    case FESC:
      uart_putchar(FESC);
      uart_putchar(TFESC);
      break;
    case FEND:
      uart_putchar(FESC);
      uart_putchar(TFEND);
      break;
    default:
      uart_putchar(rf_rxbuf[i]);
    }
  }

  /* last or single frame end */
  if ( rf_rxbuf[0] == FRAME_LAST || rf_rxbuf[0] > FRAME_FIRST )
  {
    uart_putchar(FEND);
    flag.kiss_start_sent = false;
  }
}

static void csma_transmit()
{
  uint16_t len;
  uint8_t payload_sz = RF96_MTU;

  if ( (flag.txdelay_engaged || flag.slottime_engaged) && tim2_cnt )
  {
    /* TxDelay or SlotTime is not expired */
    return;
  }

  /* Delay is expired */
  if ( flag.txdelay_engaged || flag.slottime_engaged )
  {
    flag.txdelay_engaged = false;

    if ( flag.slottime_engaged )
    {
      flag.slottime_engaged = false;

      if ( IRQ_CHECK() || MODEMSTAT_CHECK() )
      {
        /* Oops, are we still receiving? */
        return;
      }
    }
    
    /* first byte is KISS CMD */
    if ( pkt_len > 1 )  /* frame size >0 */
    {
      /* get rid of KISS CMD byte */
      CBUF_AdvancePopIdx( urxcbuf );
      len = pkt_len-1;
      if ( flag.header_implicit )
        payload_sz--;

      if ( pkt_len > payload_sz )
      {
        flag.rf_multiframe = true;
        payload_sz--;
      }
      else
        flag.rf_multiframe = false;

      flag.kiss_frame_first = true;
      while ( len > payload_sz )
      {
        len -= payload_sz;
        if ( flag.kiss_frame_first )
        {
          flag.kiss_frame_first = false;
          rf_send(rf_txbuf, assemble(payload_sz, FRAME_FIRST)); // blocking op
        }
        else
          rf_send(rf_txbuf, assemble(payload_sz, FRAME_INTER)); // blocking op
      }
      rf_send(rf_txbuf, assemble(len, FRAME_LAST)); // blocking op
      /* Return to RX */
      flag.kiss_frame_ready = false;
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
    else {
      flag.kiss_frame_ready = false;
    }
  }
  else {
    // p-persistence CSMA
    if ( random() <= P_PARAMETER )
    {
      flag.txdelay_engaged = true;
      tim2_restart(TXDELAY);
    } else
    {
      flag.slottime_engaged = true;
      tim2_restart(SLOTTIME);
    }
  }
}

static void kiss_process_cmd()
{
  // first byte is KISS cmd
  switch ( CBUF_Get( urxcbuf, 0 ) )
  {
  case CMD_DATA_FRAME:
    csma_transmit();
    break;
  case CMD_TX_DELAY:
    cmd_set_txdelay();
    flag.kiss_frame_ready = false;
    break;
  case CMD_P:
    cmd_set_p();
    flag.kiss_frame_ready = false;
    break;
  case CMD_SLOT_TIME:
    cmd_set_slottime();
    flag.kiss_frame_ready = false;
    break;
  default:
    flag.kiss_frame_ready = false;
    /* Pop out unprocessed packet data */
    urxcbuf.m_getIdx += pkt_len;
  }
}

static void cmd_set_txdelay()
{
  if ( pkt_len == 2 )
  {
    eeprom_write_byte(CFG_TXD_IDX, rf_txbuf[1]);
  }
}

static void cmd_set_p()
{
  if ( pkt_len == 2 )
  {
    eeprom_write_byte(CFG_PPR_IDX, rf_txbuf[1]);
  }
}

static void cmd_set_slottime()
{
  if ( pkt_len == 2 )
  {
    eeprom_write_byte(CFG_SLT_IDX, rf_txbuf[1]);
  }
}
