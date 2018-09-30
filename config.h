// config.h
#ifndef CONFIG_H
#define CONFIG_H

#include <iostm8l151g6.h>
#include <stdint.h>
#include <stdbool.h>
#include "CBUF.h"
#include "util.h"

#define HIGH 1
#define LOW  0


#define F_MASTER 16     /* MCU clock */
#define RF_XTAL 32000000.0      /* SX1278 crystal frq */
#define RF_FIFO_TXBASE 0x00
#define RF_FIFO_RXBASE 0x00
#define RF96_MTU        255             /* LoRa MTU */
#define RFBUF_SZ        RF96_MTU + 1    /* radio buf size */
#define urxcbuf_SIZE    1024
#define utmpbuf_SIZE    32
#define TTBL_SZ         ( utmpbuf_SIZE / 10 ) /* tmp TX buffer low watermark*/
#define TTBH_SZ         ( utmpbuf_SIZE - 10 ) /* tmp TX buffer high watermark*/
#define TBL_SZ          ( urxcbuf_SIZE / 10 ) /* TX buffer low watermark*/
#define TBH_SZ          ( urxcbuf_SIZE - 10 ) /* TX buffer high watermark*/
#define CFG_SZ          24
#define MODE_SETUP              0       /* TNC SETUP MODE */
#define MODE_BRIDGE             1       /* TNC TRANSPARENT MODE */
#define MODE_KISS               2       /* TNC KISS MODE */
#define P_PARAMETER     cfg[CFG_PPR_IDX]
#define TXDELAY         cfg[CFG_TXD_IDX]        /* in 10 ms units */
#define SLOTTIME        cfg[CFG_SLT_IDX]        /* in 10 ms units */

#define LNA_CTL         PB_ODR_ODR2
#define BIAS_CTL        PC_ODR_ODR3
#define NRST_CTL        PC_ODR_ODR4
#define M0_SW           PC_IDR_IDR5
#define M1_SW           PC_IDR_IDR6
#define AUX             PD_ODR_ODR0
#define PA_CTL          PD_ODR_ODR1
#define TR_SW10         PD_ODR_ODR2 /* TX/RX switch, STM8 pin 10 */
#define TR_SW11         PD_ODR_ODR3 /* TX/RX switch, STM8 pin 11 */
#define TCXO_CTL        PD_ODR_ODR4

#define PA_ENABLED      HIGH
#define PA_DISABLED     LOW
#define LNA_ENABLED     HIGH
#define LNA_DISABLED    LOW
#define TCXO_ENABLED    HIGH
#define TCXO_DISABLED   LOW
#define BIAS_ON         HIGH
#define BIAS_OFF        LOW

#define CFG_TNC_IDX 0  /* TNC parameters: mode, serial speed */
#define CFG_RND_IDX 1  /* salt for random generator */
#define CFG_TXD_IDX 2  /* TXDelay CSMA parameter */
#define CFG_PPR_IDX 3  /* P-persistence CSMA parameter */
#define CFG_SLT_IDX 4  /* SlotTime CSMA parameter */
#define CFG_FRU_IDX 5  /* freq. MSByte */
#define CFG_FRM_IDX 6  /* freq. mid byte */
#define CFG_FRL_IDX 7  /* freq. LSbyte */
#define CFG_PAC_IDX 8  /* PA config */
#define CFG_PAR_IDX 9  /* PA ramp */
#define CFG_OCP_IDX 10 /* overcurrent protection */
#define CFG_LNA_IDX 11 /* LNA cfg */
#define CFG_MC1_IDX 12 /* RegModemConfig1 */
#define CFG_MC2_IDX 13 /* RegModemConfig2 */
#define CFG_MC3_IDX 14 /* RegModemConfig3 */
#define CFG_PRU_IDX 15 /* preamble length MSByte */
#define CFG_PRL_IDX 16 /* -//- LSByte */
#define CFG_PPM_IDX 17 /* PPM correction */
#define CFG_DEO_IDX 18 /* DetectOptimize */
#define CFG_IIQ_IDX 19 /* InvertIq */
#define CFG_DTT_IDX 20 /* DetectionThreshold */
#define CFG_SCW_IDX 21 /* SyncWord */
#define CFG_R36_IDX 22 /* Register 0x36 (See SX127x Errata note, section 2.1.) */
#define CFG_R3A_IDX 23 /* Register 0x3A (See SX127x Errata note, section 2.1.) */

#define CFG_TNC_SSP_MASK    0x07
#define CFG_TNC_MOD_MASK    0x18

//-------------- Macros -----------------
#define BIT(n)  ( 1<<(n) )
#define BIT_SET(y, mask)        ( y |= (mask) )
#define BIT_CLELAR(y, mask)     ( y &= ~(mask) )
#define BIT_FLIP(y, mask)       ( y ^= (mask) )
//! Create a bitmask of length
#define BIT_MASK(len)           ( BIT(len)-1 )
//! Create a bitfield mask of length starting at bit
#define BF_MASK(start, len)     ( BIT_MASK(len)<<(start) )
//! Prepare a bitmask for insertion or combining
#define BF_PREP(x, start, len)  ( ((x)&BIT_MASK(len)) << (start) )
//! Extract a bitfield of length starting at bit from y
#define BF_GET(y, start, len)   ( ((y)>>(start)) & BIT_MASK(len) )
//! Insert a new bitfield value x into y
#define BF_SET(y, x, start, len)    \
    ( y= ((y) &~ BF_MASK(start, len)) | BF_PREP(x, start, len) )
//! Massage x for use in bitfield
#define BFN_PREP(x, name)    ( ((x)<<name##_SHIFT) & name##_MASK )
//! Get the value of bitfield from y. Equivalent to (var=) y.name
#define BFN_GET(y, name)     ( ((y) & name##_MASK)>>name##_SHIFT )
//! Set bitfield name from y to x: y.name= x.
#define BFN_SET(y, x, name)  (y = ((y)&~name##_MASK) | BFN_PREP(x,name) )

#define __wait_for_interrupt() asm("wfi")
#define __disable_interrupt() asm("sim")
#define __enable_interrupt() asm("rim")
#define __halt() asm("halt")
#define __reset() asm("dc8 $75") /* Initiate MCU RESET by illegal opcode */
#define HIBYTE(x) ((char*)(&(x)))[0]
#define LOBYTE(x) ((char*)(&(x)))[1]

#define TX_OFF  \
      PA_CTL = PA_DISABLED; \
      BIAS_CTL = BIAS_OFF; \
      TR_SW10 = LOW; \
      TR_SW11 = LOW
#define TX_ON   \
      RX_OFF; \
      TR_SW10 = HIGH; \
      TR_SW11 = LOW; \
      PA_CTL = PA_ENABLED; \
      BIAS_CTL = BIAS_ON
#define RX_OFF  \
      LNA_CTL = LNA_DISABLED; \
      TR_SW10 = LOW; \
      TR_SW11 = LOW
#define RX_ON   \
      TX_OFF; \
      TR_SW11 = HIGH; \
      TR_SW10 = LOW; \
      LNA_CTL = LNA_ENABLED

#define RTS_SET_HIGH \
        AUX = flag.rts_inverted ? HIGH : LOW;
#define RTS_SET_LOW \
        AUX = flag.rts_inverted ? LOW : HIGH;

extern uint32_t seed;
extern struct flag_t flag;
extern volatile struct urxcbuf_t urxcbuf;
extern volatile struct utmpbuf_t utmpbuf;
extern char rf_rxbuf[RFBUF_SZ];
extern char rf_txbuf[RFBUF_SZ];
extern uint8_t *cfg;

struct flag_t {
  unsigned char uart_rx_complete        : 1; /* UART IDLE char received */
  unsigned char uart_timer_restarted    : 1; /* serial symbol timeout */
  unsigned char uart_timer_pending      : 1; /* serial symbol timeout */
  unsigned char uart_rxne               : 1; /* UART RX buf not empty */
  unsigned char txdelay_engaged         : 1;
  unsigned char slottime_engaged        : 1;
  unsigned char tnc_mode                : 2;
  unsigned char kiss_sof                : 1; /* Start of Frame */
  unsigned char kiss_payload            : 1;
  unsigned char kiss_escaped            : 1;
  unsigned char kiss_frame_ready        : 1;
  unsigned char kiss_frame_first        : 1;
  unsigned char kiss_start_sent         : 1;
  unsigned char header_implicit         : 1;
  unsigned char rf_multiframe           : 1;
  unsigned char rts_inverted            : 1; /* RTS output inverted */
  unsigned char tfc_timeout             : 1; /* traffic timeout */
  unsigned char t1s_reset               : 1; /* 1s timer reset */
};

/* UART RX circular buffer */
struct urxcbuf_t {
    uint16_t     m_getIdx;
    uint16_t     m_putIdx;
    uint8_t      m_entry[ urxcbuf_SIZE ];
};

/* tmp UART RX circular buffer */
struct utmpbuf_t {
    uint16_t     m_getIdx;
    uint16_t     m_putIdx;
    uint8_t      m_entry[ utmpbuf_SIZE ];
};

void reset_radio(void);
void eeprom_populate(void);
void eeprom_write_byte(uint8_t, uint8_t);

#endif
