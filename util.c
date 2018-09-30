#include "config.h"
#include "util.h"

uint8_t random()
{
  seed ^= seed << 13;
  seed ^= seed >> 17;
  seed ^= seed << 5;
  return (uint8_t)(seed >> 8);
}

void eeprom_write_byte(uint8_t idx, uint8_t val)
{
  if ( idx < CFG_SZ )
  {
    FLASH_DUKR = 0xAE;
    FLASH_DUKR = 0x56;
    cfg[idx] = val;
    while(!FLASH_IAPSR_EOP);
    FLASH_IAPSR_DUL = 0;
  }
}

uint8_t assemble(uint8_t len, uint8_t id)
{
  uint8_t i, idx = 0;

  if ( flag.header_implicit )
    idx++;
  if ( flag.rf_multiframe )
    idx++;

  if ( flag.header_implicit )
    rf_txbuf[0] = len;

  if ( flag.rf_multiframe )
  {
    rf_txbuf[idx-1] = id;
  }
      
  for(i = idx; i < len+idx; i++)
  {
    rf_txbuf[i] = cbuf_pop();
  }

  return len+idx;
}

char cbuf_pop2(void)
{
  if ( CBUF_Len(utmpbuf) < TTBL_SZ )
    RTS_SET_LOW;
  return CBUF_Pop(utmpbuf);
}

char cbuf_pop(void)
{
  if ( CBUF_Len(urxcbuf) < TBL_SZ )
    RTS_SET_LOW;
  return CBUF_Pop(urxcbuf);
}

void cbuf_push(char c)
{
  /* Hardware flow control */
  if ( CBUF_Len(urxcbuf) >= TBH_SZ )
  {
    RTS_SET_HIGH;
  }
  CBUF_Push(urxcbuf, c);
}
