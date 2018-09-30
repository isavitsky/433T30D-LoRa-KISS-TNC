// STM8L 433T30D KISS TNC
// Version 1.0: June-July 2018 (c) UR5VIB
//      * initial release for RFM98
// Version 1.1 September 2018
//      * added UART hardware flow control
//      * improved performance
//
// E32-TTL-1W model 433T30D
// Due to poor system design it requires UART hardware flow ctl.
// DIOx are unpopulated on the PCB which impacts performance.
//
// STM8L pinout:
// 1 - NRST 8  - AUX    15 -        22 -
// 2 - TX   9  - PA_ON  16 - NSS    23 -
// 3 - RX   10 - RX_ON  17 - SCK    24 - BIAS
// 4 -      11 - TX_ON  18 - MOSI   25 - RST
// 5 -      12 -        19 - MISO   26 - M0
// 6 -      13 -        20 - TCX_ON 27 - M1
// 7 -      14 - LNA_ON 21 -        28 - SWIM

#include "config.h"
#include "timer.h"
#include "uart.h"
#include "spi.h"
#include "rfm98w.h"
#include "menue.h"
#include "bridge.h"
#include "kiss.h"

struct flag_t flag;
volatile struct urxcbuf_t urxcbuf;
volatile struct utmpbuf_t utmpbuf;
char rf_rxbuf[RFBUF_SZ];
char rf_txbuf[RFBUF_SZ];

uint8_t *cfg = (uint8_t *) 0x1000;        //  EEPROM base address.

static void mcu_init(void);
static void modemcfg_populate(void);
void reset_radio(void);

uint32_t seed;

void main( void )
{
  mcu_init();

  // TTL or RS-232 flow ctl
  if ( M1_SW == HIGH )
    flag.rts_inverted = true;
  else
    flag.rts_inverted = false;
  
  TCXO_CTL = TCXO_ENABLED;
  spi_init();
  tim2_start();

  // Reset SX1278
  reset_radio();
  // Set LoRa mode
  rf_setopmode(RF98_MODE_SLEEP);
  rf_setopmode(RF98_LONG_RANGE_MODE);

  // Load config into the radio
  modemcfg_populate();

  /* Update random() salt */
  rf_setopmode(RF98_MODE_RXCONTINUOUS);
  delay_10ms(1);
  seed = ((uint32_t)cfg[CFG_RND_IDX] << 16);
  seed |= (spi_readreg(RF98_REG_1B_RSSI_VALUE) << 8);
  seed |= spi_readreg(RF98_REG_2C_RSSI_WIDEBAND);
  rf_setopmode(RF98_MODE_SLEEP);
  if ( seed == 0 )
    seed = 0x07; /* kludge */
  /* save it for next use */
  if ( (uint8_t)seed )
  {
    eeprom_write_byte(CFG_RND_IDX, (uint8_t)seed);
  }

  /* Set initial TNC mode according to switch positon */
  if ( M0_SW == HIGH )
  {
    flag.tnc_mode = ((cfg[CFG_TNC_IDX] & CFG_TNC_MOD_MASK) >> 3);
  } else
  {
    flag.tnc_mode = MODE_SETUP;
  }

  /* Main programme loop */
  while(1)
  {
    switch ( flag.tnc_mode )
    {
    case MODE_SETUP:
      menue();
      break;
    case MODE_BRIDGE:
      bridge();
      break;
    case MODE_KISS:
      kiss();
      break;
    default:
      __wait_for_interrupt();
    }
  }
}

void mcu_init()
{
  CLK_CKDIVR = 0;   // 16 MHz
  
  /* Configure unused pins as Out Push-Pull, Low. See RefMan. sect. 10.5. */
  PA_DDR = 0xFF;        // OUT
  PA_CR1 = 0xFF;        // P-P
  PA_ODR = 0x00;        // LOW
  PB_DDR = 0xFF;        // OUT
  PB_CR1 = 0xFF;        // P-P
  PB_ODR = 0x00;        // LOW
  PC_DDR = 0xFF;        // OUT
  PC_CR1 = 0xFF;        // P-P
  PC_ODR = 0x00;        // LOW
  PD_DDR = 0xFF;        // OUT
  PD_CR1 = 0xFF;        // P-P
  PD_ODR = 0x00;        // LOW
  
  /* PA1 = RESET */
  PA_DDR_DDR1 = 0;      // IN
  PA_CR1_C11 = 1;       // P-U

  /* PB4 = CS */
  PB_DDR_DDR4 = 1;        // OUT
  PB_CR1_C14 = 1;         // PUSH-PULL
  PB_ODR_ODR4 = HIGH;     // HIGH
  /* PB5 = SCK */
  PB_DDR_DDR5 = 1;        // OUT
  PB_CR1_C15 = 1;         // PUSH-PULL
  PB_CR2_C25 = 1;         // 10 MHz
  PB_ODR_ODR5 = LOW;      // LOW
  /* PB6 = MOSI */
  PB_DDR_DDR6 = 1;        // OUT
  PB_CR1_C16 = 1;         // PUSH-PULL
  PB_CR2_C26 = 1;         // 10 MHz
  PB_ODR_ODR6 = LOW;      // LOW
  /* PB7 = MISO */
  PB_DDR_DDR7 = 0;        // IN
  PB_CR1_C17 = 1;         // PULL-UP
  
  /* PB2 = LNA_CTL */
  PB_DDR_DDR2 = 1;        // OUT
  PB_CR1_C12 = 1;         // PUSH-PULL
  /* PC3 = PA BIAS_CTL */
  PC_DDR_DDR3 = 1;        // OUT
  PC_CR1_C13 = 1;         // PUSH-PULL
  /* PC4 = NRST_CTL */
  PC_DDR_DDR4 = 0;        // IN
  PC_CR1_C14 = 0;         // FLOAT
  /* PC5 = M0_SW */
  PC_DDR_DDR5 = 0;        // IN
  PC_CR1_C15 = 0;         // FLOAT
  PC_CR2_C25 = 1;         // EXTI
  EXTI_CR2_P5IS = 3;      // int on falling & rising edge
  /* PC6 = M1_SW */
  PC_DDR_DDR6 = 0;        // IN
  PC_CR1_C16 = 0;         // FLOAT
  PC_CR2_C26 = 1;         // EXTI
  EXTI_CR2_P6IS = 3;      // int on falling & rising edge
  /* PD0 = AUX */
  PD_DDR_DDR0 = 1;        // OUT
  PD_CR1_C10 = 1;         // PUSH-PULL
  /* PD1 = PA_CTL */
  PD_DDR_DDR1 = 1;        // OUT
  PD_CR1_C11 = 1;         // PUSH-PULL
  /* PD2 = TR_SW10 TX/RX Switch */
  PD_DDR_DDR2 = 1;        // OUT
  PD_CR1_C12 = 1;         // PUSH-PULL
  /* PD3 = TR_SW11 TX/RX Switch */
  PD_DDR_DDR3 = 1;        // OUT
  PD_CR1_C13 = 1;         // PUSH-PULL
  /* PD4 = TCXO_CTL */
  PD_DDR_DDR4 = 1;        // OUT
  PD_CR1_C14 = 1;         // PUSH-PULL

  __enable_interrupt();
}

void eeprom_populate()
{
  reset_radio();
  rf_setopmode(RF98_MODE_SLEEP);
  rf_setopmode(RF98_LONG_RANGE_MODE);

  eeprom_write_byte(CFG_FRU_IDX, spi_readreg(RF98_REG_06_FRF_MSB));
  eeprom_write_byte(CFG_FRM_IDX, spi_readreg(RF98_REG_07_FRF_MID));
  eeprom_write_byte(CFG_FRL_IDX, spi_readreg(RF98_REG_08_FRF_LSB));
  eeprom_write_byte(CFG_PAC_IDX, spi_readreg(RF98_REG_09_PA_CONFIG));
  eeprom_write_byte(CFG_PAR_IDX, spi_readreg(RF98_REG_0A_PA_RAMP));
  eeprom_write_byte(CFG_OCP_IDX, spi_readreg(RF98_REG_0B_OCP));
  eeprom_write_byte(CFG_LNA_IDX, spi_readreg(RF98_REG_0C_LNA));
  eeprom_write_byte(CFG_MC1_IDX, spi_readreg(RF98_REG_1D_MODEM_CONFIG1));
  eeprom_write_byte(CFG_MC2_IDX, spi_readreg(RF98_REG_1E_MODEM_CONFIG2));
  eeprom_write_byte(CFG_MC3_IDX, spi_readreg(RF98_REG_26_MODEM_CONFIG3));
  eeprom_write_byte(CFG_PRU_IDX, spi_readreg(RF98_REG_20_PREAMBLE_MSB));
  eeprom_write_byte(CFG_PRL_IDX, spi_readreg(RF98_REG_21_PREAMBLE_LSB));
  eeprom_write_byte(CFG_PPM_IDX, spi_readreg(RF98_REG_27_PPM_CORRECTION));
  eeprom_write_byte(CFG_DEO_IDX, spi_readreg(RF98_REG_31_DETECT_OPTIMIZ));
  eeprom_write_byte(CFG_IIQ_IDX, spi_readreg(RF98_REG_33_INVERT_IQ));
  eeprom_write_byte(CFG_DTT_IDX, spi_readreg(RF98_REG_37_DETECTION_THRESHOLD));
  eeprom_write_byte(CFG_SCW_IDX, spi_readreg(RF98_REG_39_SYNC_WORD));
  eeprom_write_byte(CFG_R36_IDX, spi_readreg(RF98_REG_36));
  eeprom_write_byte(CFG_R3A_IDX, spi_readreg(RF98_REG_3A));
}

void modemcfg_populate()
{
  rf_setopmode(RF98_MODE_STDBY);
  spi_writereg(RF98_REG_06_FRF_MSB, cfg[CFG_FRU_IDX]);
  spi_writereg(RF98_REG_07_FRF_MID, cfg[CFG_FRM_IDX]);
  spi_writereg(RF98_REG_08_FRF_LSB, cfg[CFG_FRL_IDX]);
  spi_writereg(RF98_REG_09_PA_CONFIG, cfg[CFG_PAC_IDX]);
  spi_writereg(RF98_REG_0A_PA_RAMP, cfg[CFG_PAR_IDX]);
  spi_writereg(RF98_REG_0B_OCP, cfg[CFG_OCP_IDX]);
  spi_writereg(RF98_REG_0C_LNA, cfg[CFG_LNA_IDX]);
  spi_writereg(RF98_REG_1D_MODEM_CONFIG1, cfg[CFG_MC1_IDX]);
  spi_writereg(RF98_REG_1E_MODEM_CONFIG2, cfg[CFG_MC2_IDX]);
  spi_writereg(RF98_REG_26_MODEM_CONFIG3, cfg[CFG_MC3_IDX]);
  spi_writereg(RF98_REG_20_PREAMBLE_MSB, cfg[CFG_PRU_IDX]);
  spi_writereg(RF98_REG_21_PREAMBLE_LSB, cfg[CFG_PRL_IDX]);
  spi_writereg(RF98_REG_27_PPM_CORRECTION, cfg[CFG_PPM_IDX]);
  spi_writereg(RF98_REG_31_DETECT_OPTIMIZ, cfg[CFG_DEO_IDX]);
  spi_writereg(RF98_REG_33_INVERT_IQ, cfg[CFG_IIQ_IDX]);
  spi_writereg(RF98_REG_37_DETECTION_THRESHOLD, cfg[CFG_DTT_IDX]);
  spi_writereg(RF98_REG_36, cfg[CFG_R36_IDX]);
  if ( spi_readreg(RF98_REG_36) == RF98_REG_36_500KHZ )
  {
    spi_writereg(RF98_REG_3A, cfg[CFG_R3A_IDX]);
  }
}

void reset_radio()
{
  uint8_t tim2_state = TIM2_CR1_CEN;

  tim2_start();
  delay_10ms(1);
  PC_DDR_DDR4 = 0;      // IN
  PC_CR1_C14 = 0;       // FLOAT

  delay_10ms(1);
  PC_DDR_DDR4 = 1;      // OUT
  PC_CR1_C14 = 1;       // P-P
  
  NRST_CTL = LOW;      // PULL LOW
  delay_10ms(1);
  NRST_CTL = HIGH;      // PULL HIGH
  delay_10ms(1);

  PC_DDR_DDR4 = 0;      // IN
  PC_CR1_C14 = 0;       // FLOAT

  /* return TIM2 previous state */
  if ( tim2_state == 0 )
  {
    tim2_stop();
  }
}
