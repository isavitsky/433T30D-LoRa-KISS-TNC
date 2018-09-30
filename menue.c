#include "config.h"
#include "menue.h"
#include "uart.h"
#include "rfm98w.h"
#include "spi.h"
#include "timer.h"

#define SIO_SP_SZ       8
#define DIG_SZ          10
#define SF_SZ           7
#define CR_SZ           4
#define BW_SZ           10
#define RAMP_SZ         16
#define LNA_SZ          6

static char input;
static char dig[DIG_SZ];

char *sio_speeds[SIO_SP_SZ] = {
    "9600",
    "19200",
    "38400",
    "57600",
    "115200",
    "230400",
    "460800",
    "921600" };

static char *sf_levels[SF_SZ] = {
    "6 (64 CPS)",
    "7 (128 CPS)",
    "8 (256 CPS)",
    "9 (512 CPS)",
    "10 (1024 CPS)",
    "11 (2048 CPS)",
    "12 (4096 CPS)" };

static char *cr_levels[CR_SZ] = {
    "4/5 (x 1.25 overhead)",
    "4/6 (x 1.5 overhead)",
    "4/7 (x 1.75 overhead)",
    "4/8 (x 2 overhead)" };

static char *bw_levels[BW_SZ] = {
    "7.8 kHz",
    "10.4 kHz",
    "15.6 kHz",
    "20.8 kHz",
    "31.25 kHz",
    "41.7 kHz",
    "62.5 kHz",
    "125 kHz",
    "250 kHz",
    "500 kHz" };

static char *ramp_levels[RAMP_SZ] = {
  "0000 -> 3.4 ms",
  "0001 -> 2 ms",
  "0010 -> 1 ms",
  "0011 -> 500 us",
  "0100 -> 250 us",
  "0101 -> 125 us",
  "0110 -> 100 us",
  "0111 -> 62 us",
  "1000 -> 50 us",
  "1001 -> 40 us",
  "1010 -> 31 us",
  "1011 -> 25 us",
  "1100 -> 20 us",
  "1101 -> 15 us",
  "1110 -> 12 us",
  "1111 -> 10 us" };

static char *lna_levels[LNA_SZ] = {
  "001 -> G1 = maximum gain",
  "010 -> G2",
  "011 -> G3",
  "100 -> G4",
  "101 -> G5",
  "110 -> G6 = minimum gain" };

uint32_t atoi_simple(char *str)
{
    uint32_t res = 0;
  
    for (uint8_t i = 0; str[i] != '\0'; ++i)
        res = res*10 + str[i] - '0';
    return res;
}

void itob_simple(char *dst, uint8_t x)
{
  uint8_t i=7;
  do
  {
    *dst++ = (x >> i & 1) ? '1' : '0';
  } while(i--);
  *dst = '\0';
}
/*
static void itox_simple(char *s, uint8_t i)
{
  uint8_t n;
  s += 4;
  *s = '\0';
  for (n = 4; n != 0; --n) {
    *--s = "0123456789ABCDEF"[i & 0x0F];
    i >>= 4;
  }
}
*/

static void itox_simple(char *s, uint8_t i)
{
  char hex[] = "0123456789ABCDEF";
  
  s[0] = hex[i >> 4];
  s[1] = hex[i & 0x0F];
  s[2] = 0;
}

static char *itoa_simple_helper(char *dest, long int i) {
  if (i <= -10) {
    dest = itoa_simple_helper(dest, i/10);
  }
  *dest++ = '0' - i%10;
  return dest;
}

char *itoa_simple(char *dest, long int i) {
  char *s = dest;
  if (i < 0) {
    *s++ = '-';
  } else {
    i = -i;
  }
  *itoa_simple_helper(s, i) = '\0';
  return dest;
}

static void clearScreen()
{
  uart_putchar(27);     // ESC
  uart_puts("[2J");     // clear screen
}

static void goHome()
{
  uart_putchar(27);     // ESC
  uart_puts("[H");      // cursor to home
}

static void menue_basic_serial_draw()
{
  uint8_t i;
  
  clearScreen();
  goHome();
  uart_println("TNC serial port speed:");
  uart_println("");
  for (i=0;i<SIO_SP_SZ;i++)
  {
    if ( (cfg[CFG_TNC_IDX] & CFG_TNC_SSP_MASK) == i )
      uart_putchar('+');
    else
      uart_putchar(' ');
    uart_putchar(i+'1');
    uart_puts(" - ");
    uart_println(sio_speeds[i]);
  }
  uart_println(" [ENTER] - Return to the previous menue");
}

static void menue_basic_serial()
{
  uint8_t t;
  do
  {
    menue_basic_serial_draw();
    __wait_for_interrupt();
    if ( flag.uart_rxne )
    {
      input = uart_getc();
      if ( (input >= '1') && (input < ('1' + SIO_SP_SZ)) )
      {
        t = cfg[CFG_TNC_IDX] & ~(CFG_TNC_SSP_MASK);
        t |= (uint8_t)(input - '1');
        eeprom_write_byte(CFG_TNC_IDX, t);
      }
    }
  } while ( (flag.tnc_mode == MODE_SETUP) && (input != '\r') );
  input = '\0';
}

static void menue_basic_mode_draw()
{
  uint8_t mode;
  
  clearScreen();
  goHome();
  uart_println("TNC mode:");
  uart_println("");
  mode = (cfg[CFG_TNC_IDX] & CFG_TNC_MOD_MASK) >> 3;
  ( mode == MODE_BRIDGE ) ? uart_putchar('+') : uart_putchar(' ');
  uart_println("1 - Transparent RF UART bridge");
  ( mode == MODE_KISS ) ? uart_putchar('+') : uart_putchar(' ');
  uart_println("2 - KISS TNC");
  uart_println(" [ENTER] - Return to the previous menue");
}

static void menue_basic_mode()
{
  uint8_t mode;

  do
  {
    menue_basic_mode_draw();
    __wait_for_interrupt();
    if ( flag.uart_rxne )
    {
      input = uart_getc();
      switch ( input )
      {
      case '1':
        mode = cfg[CFG_TNC_IDX] & ~(CFG_TNC_MOD_MASK);
        mode |= (MODE_BRIDGE << 3);
        eeprom_write_byte(CFG_TNC_IDX, mode);
        break;
      case '2':
        mode = cfg[CFG_TNC_IDX] & ~(CFG_TNC_MOD_MASK);
        mode |= (MODE_KISS << 3);
        eeprom_write_byte(CFG_TNC_IDX, mode);
        break;
      }
    }
  } while ( (flag.tnc_mode == MODE_SETUP) && (input != '\r') );
  input = '\0';
}

static void menue_basic_freq_draw()
{
  uint32_t frq_i;

  clearScreen();
  goHome();
  uart_println("Modem frequency:");
  uart_println("");
  uart_puts("Current frequency: ");
  frq_i = (uint32_t)(rf_getfreq() * RF_STEP / 1000.0 );
  itoa_simple(dig, frq_i);
  uart_puts(dig);
  uart_println(" kHz");
  uart_puts("Enter new frequency in kHz or hit [ENTER] to return: ");
}

static void menue_basic_freq()
{
  uint32_t frq_i, frq_s;

  do
  {
    menue_basic_freq_draw();
    uart_readln(dig, DIG_SZ);
    frq_i = atoi_simple(dig);
    if ( frq_i != 0 )
    {
      frq_s = (uint32_t)(frq_i * 1000.0 / RF_STEP);
      rf_setfreq(frq_s);
      eeprom_write_byte(CFG_FRU_IDX, (uint8_t)(frq_s >> 16));
      eeprom_write_byte(CFG_FRM_IDX, (uint8_t)(frq_s >> 8));
      eeprom_write_byte(CFG_FRL_IDX, (uint8_t)frq_s);
    }
  } while ( (flag.tnc_mode == MODE_SETUP) && (dig[0] != '\0') );
}

static void menue_basic_sf_draw()
{
  uint8_t i;
  
  clearScreen();
  goHome();
  uart_println("Modem spreading factor in chips per symbol:");
  uart_println("");
  for (i=0;i<SF_SZ;i++)
  {
    /* bit field SpreadingFactor of RegModemCfg2, starts with 6 */
    if ( ((spi_readreg(RF98_REG_1E_MODEM_CONFIG2) >> 4) & 0x0F) == (i+6) )
      uart_putchar('+');
    else
      uart_putchar(' ');
    uart_putchar(i+'1');
    uart_puts(" - ");
    uart_println(sf_levels[i]);
  }
  uart_println(" [ENTER] - Return to the previous menue");
}

static void menue_basic_sf()
{
  uint8_t t;

  do
  {
    menue_basic_sf_draw();
    __wait_for_interrupt();
    if ( flag.uart_rxne )
    {
      input = uart_getc();

      t = spi_readreg(RF98_REG_1E_MODEM_CONFIG2) &
        ~(RF98_SPREADING_FACTOR_MASK);
      if ( input == '1' )
      {
        /* SF6 is a special case, see datasheet section 4.1.1.2. */
        /* set SF */
        t |= RF98_SPREADING_FACTOR_64CPS;
        t |= RF98_PAYLOAD_CRC_ON;       // we need CRC
        spi_writereg(RF98_REG_1E_MODEM_CONFIG2, t);
        eeprom_write_byte(CFG_MC2_IDX, t);
        /* set implicit header */
        t = spi_readreg(RF98_REG_1D_MODEM_CONFIG1) &
          ~(RF98_IMPLICIT_HEADER_MODE_ON_MASK);
        t |= RF98_IMPLICIT_HEADER_MODE_ON;
        spi_writereg(RF98_REG_1D_MODEM_CONFIG1, t);
        eeprom_write_byte(CFG_MC1_IDX, t);
        /* set DetectOptimize */
        t = spi_readreg(RF98_REG_31_DETECT_OPTIMIZ);
        t &= ~(RF98_DETOPTIMIZE_MASK);
        t |= RF98_DETOPTIMIZE_05;
        spi_writereg(RF98_REG_31_DETECT_OPTIMIZ, t);
        eeprom_write_byte(CFG_DEO_IDX, t);
        /* set DetectionThreshold */
        spi_writereg(RF98_REG_37_DETECTION_THRESHOLD,
                        RF98_DETTH_0C);
        eeprom_write_byte(CFG_DTT_IDX, RF98_DETTH_0C);
      } else if ( (input >= '2') && (input <= '7') )
      {
        /* set SF */
        switch ( input )
        {
        case '2':
          t |= RF98_SPREADING_FACTOR_128CPS;
          t |= RF98_PAYLOAD_CRC_ON;     // we need CRC
          spi_writereg(RF98_REG_1E_MODEM_CONFIG2, t);
          eeprom_write_byte(CFG_MC2_IDX, t);
          break;
        case '3':
          t |= RF98_SPREADING_FACTOR_256CPS;
          t |= RF98_PAYLOAD_CRC_ON;     // we need CRC
          spi_writereg(RF98_REG_1E_MODEM_CONFIG2, t);
          eeprom_write_byte(CFG_MC2_IDX, t);
          break;
        case '4':
          t |= RF98_SPREADING_FACTOR_512CPS;
          t |= RF98_PAYLOAD_CRC_ON;     // we need CRC
          spi_writereg(RF98_REG_1E_MODEM_CONFIG2, t);
          eeprom_write_byte(CFG_MC2_IDX, t);
          break;
        case '5':
          t |= RF98_SPREADING_FACTOR_1024CPS;
          t |= RF98_PAYLOAD_CRC_ON;     // we need CRC
          spi_writereg(RF98_REG_1E_MODEM_CONFIG2, t);
          eeprom_write_byte(CFG_MC2_IDX, t);
          break;
        case '6':
          t |= RF98_SPREADING_FACTOR_2048CPS;
          t |= RF98_PAYLOAD_CRC_ON;     // we need CRC
          spi_writereg(RF98_REG_1E_MODEM_CONFIG2, t);
          eeprom_write_byte(CFG_MC2_IDX, t);
          break;
        case '7':
          t |= RF98_SPREADING_FACTOR_4096CPS;
          t |= RF98_PAYLOAD_CRC_ON;     // we need CRC
          spi_writereg(RF98_REG_1E_MODEM_CONFIG2, t);
          eeprom_write_byte(CFG_MC2_IDX, t);
          break;          
        }
        /* set explicit header */
        t = spi_readreg(RF98_REG_1D_MODEM_CONFIG1) &
          ~(RF98_IMPLICIT_HEADER_MODE_ON_MASK);
        spi_writereg(RF98_REG_1D_MODEM_CONFIG1, t);
        eeprom_write_byte(CFG_MC1_IDX, t);
        /* set DetectOptimize */
        t = spi_readreg(RF98_REG_31_DETECT_OPTIMIZ) &
          ~(RF98_DETOPTIMIZE_MASK);
        t |= RF98_DETOPTIMIZE_03;
        spi_writereg(RF98_REG_31_DETECT_OPTIMIZ, t);
        eeprom_write_byte(CFG_DEO_IDX, t);
        /* set DetectionThreshold */
        spi_writereg(RF98_REG_37_DETECTION_THRESHOLD,
                        RF98_DETTH_0A);
        eeprom_write_byte(CFG_DTT_IDX, RF98_DETTH_0A);
      }
    }
  } while ( (flag.tnc_mode == MODE_SETUP) && (input != '\r') );
  input = '\0';
}

static void menue_basic_cr_draw()
{
  uint8_t i;
  
  clearScreen();
  goHome();
  uart_println("Modem coding rate in symbols:");
  uart_println("");
  for (i=0;i<CR_SZ;i++)
  {
    /* bit field CodingRate of RegModemCfg1, starts with 1 */
    if ( ((spi_readreg(RF98_REG_1D_MODEM_CONFIG1) >> 1) & 0x07) == (i+1) )
      uart_putchar('+');
    else
      uart_putchar(' ');
    uart_putchar(i+'1');
    uart_puts(" - ");
    uart_println(cr_levels[i]);
  }
  uart_println(" [ENTER] - Return to the previous menue");
}

static void menue_basic_cr()
{
  uint8_t t;

  do
  {
    menue_basic_cr_draw();
    __wait_for_interrupt();
    if ( flag.uart_rxne )
    {
      input = uart_getc();
      if ( (input >= '1') && (input < ('1' + CR_SZ)) )
      {
        t = spi_readreg(RF98_REG_1D_MODEM_CONFIG1) & 0xF1;
        t |= ( ((uint8_t)(input - '1' + 1)) << 1 ); // CR starting with 1
        spi_writereg(RF98_REG_1D_MODEM_CONFIG1, t);
        eeprom_write_byte(CFG_MC1_IDX, t);
      }
    }
  } while ( (flag.tnc_mode == MODE_SETUP) && (input != '\r') );
  input = '\0';
}

static void menue_basic_bw_draw()
{
  uint8_t i;
  
  clearScreen();
  goHome();
  uart_println("Modulation bandwidth:");
  uart_println("");
  for (i=0;i<BW_SZ;i++)
  {
    /* bit field Bw of RegModemCfg1, starts with 0 */
    if ( ((spi_readreg(RF98_REG_1D_MODEM_CONFIG1) >> 4) & 0x0F) == i )
      uart_putchar('+');
    else
      uart_putchar(' ');
    uart_putchar(i+'0');
    uart_puts(" - ");
    uart_println(bw_levels[i]);
  }
  uart_println(" [ENTER] - Return to the previous menue");
}

static void menue_basic_bw()
{
  uint8_t t;

  do
  {
    menue_basic_bw_draw();
    __wait_for_interrupt();
    if ( flag.uart_rxne )
    {
      input = uart_getc();
      if ( (input >= '0') && (input < ('0' + BW_SZ)) )
      {
        t = spi_readreg(RF98_REG_1D_MODEM_CONFIG1) & 0x0F;
        t |= ( ((uint8_t)(input - '0')) << 4 ); // BW starting with 0
        spi_writereg(RF98_REG_1D_MODEM_CONFIG1, t);
        eeprom_write_byte(CFG_MC1_IDX, t);
        if ( input == '9' )
        {
          /*
          For 500 kHz bandwidth we need to apply some corrections
          to shadow registers. See SX127x Errata Note, Section 2.1.
          */
          spi_writereg(RF98_REG_36, RF98_REG_36_500KHZ);
          eeprom_write_byte(CFG_R36_IDX, RF98_REG_36_500KHZ);
          spi_writereg(RF98_REG_3A, RF98_REG_3A_500KHZ);
          eeprom_write_byte(CFG_R3A_IDX, RF98_REG_3A_500KHZ);
        } else
        {
          spi_writereg(RF98_REG_36, RF98_REG_36_DEFAULT);
          eeprom_write_byte(CFG_R36_IDX, RF98_REG_36_DEFAULT);
          //spi_writereg(RF98_REG_3A, RF98_REG_3A_DEFAULT); // not needed as per errata
          //eeprom_write_byte(CFG_R3A_IDX, RF98_REG_3A_DEFAULT);
        }
      }
    }
  } while ( (flag.tnc_mode == MODE_SETUP) && (input != '\r') );
  input = '\0';
}

static void menue_basic_draw()
{
  uint8_t t;

  clearScreen();
  goHome();
  uart_println("Basic settings:");
  uart_println("");
  uart_puts("0 - TNC operation mode: ");
  t = (cfg[CFG_TNC_IDX] & CFG_TNC_MOD_MASK) >> 3;
  switch ( t )
  {
  case MODE_BRIDGE:
    uart_println("MODE_BRIDGE");
    break;
  case MODE_KISS:
    uart_println("MODE_KISS");
    break;
  default:
    uart_println("unknown");
  }
  uart_puts("1 - TNC serial port speed: ");
  t = cfg[CFG_TNC_IDX] & CFG_TNC_SSP_MASK;
  if ( t < SIO_SP_SZ )
    uart_println(sio_speeds[t]);
  else
    uart_println("unknown");
  uart_puts("2 - TxDelay: ");
  itoa_simple(dig, TXDELAY);
  uart_puts(dig);
  uart_println(" x 10 ms");
  uart_puts("3 - P-parameter: ");
  itoa_simple(dig, P_PARAMETER);
  uart_println(dig);
  uart_puts("4 - SlotTime: ");  
  itoa_simple(dig, SLOTTIME);
  uart_puts(dig);
  uart_println(" x 10 ms");
  uart_puts("5 - Modem frequency: ");
  itoa_simple(dig, (uint32_t)(rf_getfreq() * RF_STEP / 1000.0));
  uart_puts(dig);
  uart_println(" kHz");
  uart_puts("6 - Modem spreading factor: ");
  t = spi_readreg(RF98_REG_1E_MODEM_CONFIG2);
  t &= RF98_SPREADING_FACTOR;
  t >>= 4;
  t -= 6;
  //t = ((spi_readreg(RF98_REG_1E_MODEM_CONFIG2) & RF98_SPREADING_FACTOR) >> 4) - 6;
  if ( t < SF_SZ )
    uart_println(sf_levels[t]);
  else
    uart_println("unknown");
  uart_puts("7 - Modem coding rate: ");
  t = ((spi_readreg(RF98_REG_1D_MODEM_CONFIG1) & RF98_CODING_RATE) >> 1) - 1;
  if ( t < CR_SZ )
    uart_println(cr_levels[t]);
  else
    uart_println("unknown");
  uart_puts("8 - Modulation bandwidth: ");
  t = (spi_readreg(RF98_REG_1D_MODEM_CONFIG1) & RF98_BW) >> 4;
  if ( t < BW_SZ )
    uart_println(bw_levels[t]);
  else
    uart_println("unknown");
  uart_println("[ENTER] - Return to the previous menue");
}

static void menue_basic_txdelay_draw()
{
  clearScreen();
  goHome();
  uart_println("TxDelay:");
  uart_println("");
  uart_puts("Current TxDelay (decimal): ");
  itoa_simple(dig, TXDELAY);
  uart_println(dig);
  uart_puts("Enter TxDelay in range 0..255 or hit [ENTER] to return: ");
}

static void menue_basic_txdelay()
{
  do
  {
    menue_basic_txdelay_draw();
    uart_readln(dig, DIG_SZ);
    if ( dig[0] != '\0' )
    {
      eeprom_write_byte(CFG_TXD_IDX, (uint8_t)atoi_simple(dig));
    }
  } while ( (flag.tnc_mode == MODE_SETUP) && (dig[0] != '\0') );
}

static void menue_basic_ppar_draw()
{
  clearScreen();
  goHome();
  uart_println("P-parameter:");
  uart_println("Note that p = 1 (P = 255) means 'transmit as soon as the channel clears.'");
  uart_println("The default value is P = 63 (i.e., p = 0.25).");
  uart_println("");
  uart_puts("Current P-parameter (decimal): ");
  itoa_simple(dig, P_PARAMETER);
  uart_println(dig);
  uart_puts("Enter P-parameter in range 0..255 or hit [ENTER] to return: ");
}

static void menue_basic_ppar()
{
  do
  {
    menue_basic_ppar_draw();
    uart_readln(dig, DIG_SZ);
    if ( dig[0] != '\0' )
    {
      eeprom_write_byte(CFG_PPR_IDX, (uint8_t)atoi_simple(dig));
    }
  } while ( (flag.tnc_mode == MODE_SETUP) && (dig[0] != '\0') );
}

static void menue_basic_slottime_draw()
{
  clearScreen();
  goHome();
  uart_println("SlotTime:");
  uart_println("");
  uart_puts("Current SlotTime (decimal): ");
  itoa_simple(dig, SLOTTIME);
  uart_println(dig);
  uart_puts("Enter SlotTime in range 0..255 or hit [ENTER] to return: ");
}

static void menue_basic_slottime()
{
  do
  {
    menue_basic_slottime_draw();
    uart_readln(dig, DIG_SZ);
    if ( dig[0] != '\0' )
    {
      eeprom_write_byte(CFG_SLT_IDX, (uint8_t)atoi_simple(dig));
    }
  } while ( (flag.tnc_mode == MODE_SETUP) && (dig[0] != '\0') );
}

static void menue_basic()
{
  do
  {
    menue_basic_draw();
    __wait_for_interrupt();
    if ( flag.uart_rxne )
    {
      input = uart_getc();
      switch ( input ) {
      case '0':
        menue_basic_mode();
        break;
      case '1':
        menue_basic_serial();
        break;
      case '2':
        menue_basic_txdelay();
        break;
      case '3':
        menue_basic_ppar();
        break;
      case '4':
        menue_basic_slottime();
        break;
      case '5':
        menue_basic_freq();
        break;
      case '6':
        menue_basic_sf();
        break;
      case '7':
        menue_basic_cr();
        break;
      case '8':
        menue_basic_bw();
        break;
      case '\r':
        continue;
        break;
      }
    }
  } while ( (flag.tnc_mode == MODE_SETUP) && (input != '\r') );
  input = '\0';
}

static void menue_readmodem_draw()
{
  clearScreen();
  goHome();
  uart_println("Populate TNC EEPROM data by reading modem registers");
  uart_println("(This will overwrite the previously stored settings in TNC EEPROM.)");
  uart_println("");
  uart_println("1 - Overwrite TNC EEPROM settings with modem data");
  uart_println("[ENTER] - Return to the previous menue");
}

static void menue_readmodem()
{
  bool pressed = false;
  bool m_read = false;

  do
  {
    menue_readmodem_draw();
    if ( pressed )
    {
      pressed = false;
      if ( m_read )
        uart_println("\r\nOperation completed");
      else
        uart_println("\r\nNo action performed");
    }
    __wait_for_interrupt();
    if ( flag.uart_rxne )
    {
      pressed = true;
      input = uart_getc();
      if ( input == '1' )
      {
        eeprom_populate();
        m_read = true;
      } else {
        m_read = false;
      }
    }
  } while ( (flag.tnc_mode == MODE_SETUP) && (input != '\r') );
  input = '\0';
}

static void menue_adv_draw()
{
  clearScreen();
  goHome();
  uart_println("Advanced settings (see datasheet for more info):");
  uart_println("");
  uart_println("0 - PaConfig");
  uart_println("1 - PaRamp");
  uart_println("2 - Ocp");
  uart_println("3 - Lna");
  uart_println("4 - Preamble");
  uart_println("5 - LowDatarateOptimize");
  uart_println("6 - AgcAutoOn");
  uart_println("7 - PpmCorrection");
  uart_println("8 - DetectOptimize");
  uart_println("9 - InvertIq");
  uart_println("a - DetectionThreshold");
  uart_println("b - SyncWord");
  uart_println("[ENTER] - Return to the previous menue");
}

static void menue_adv_paconfig_draw()
{
  clearScreen();
  goHome();
  uart_puts("RegPaConfig (0x09): 0x");
  itox_simple(dig, spi_readreg(RF98_REG_09_PA_CONFIG));
  uart_puts(dig);
  uart_puts(" 0b");
  itob_simple(dig, spi_readreg(RF98_REG_09_PA_CONFIG));
  uart_println(dig);
  uart_println("");
  uart_puts("1 - PaSelect: ");
  if ( spi_readreg(RF98_REG_09_PA_CONFIG) >> 7 )
    uart_println("PA_BOOST");
  else
    uart_println("RFO");
  uart_puts("2 - MaxPower: ");
  itoa_simple(dig, (spi_readreg(RF98_REG_09_PA_CONFIG) & 0x70) >> 4);
  uart_println(dig);
  uart_puts("3 - OutputPower: ");
  itoa_simple(dig, spi_readreg(RF98_REG_09_PA_CONFIG) & 0x0F);
  uart_println(dig);
  uart_println("[ENTER] - Return to the previous menue");
}

static void menue_adv_paconfig_paselect_draw()
{
  uint8_t t;
  clearScreen();
  goHome();
  uart_println("RegPaConfig PaSelect:");
  uart_println("");
  t = spi_readreg(RF98_REG_09_PA_CONFIG) >> 7;
  t ? uart_puts(" ") : uart_puts("+");
  uart_println("0 - RFO");
  t ? uart_puts("+") : uart_puts(" ");
  uart_println("1 - PA_BOOST");
  uart_println("[ENTER] - Return to the previous menue");
}

static void menue_adv_paconfig_paselect()
{
  uint8_t t;

  do
  {
    menue_adv_paconfig_paselect_draw();
    __wait_for_interrupt();
    if ( flag.uart_rxne )
    {
      input = uart_getc();
      switch ( input ) {
      case '0':
        t = spi_readreg(RF98_REG_09_PA_CONFIG) & ~(RF98_PA_SELECT);
        spi_writereg(RF98_REG_09_PA_CONFIG, t);
        eeprom_write_byte(CFG_PAC_IDX, t);
        break;
      case '1':
        t = spi_readreg(RF98_REG_09_PA_CONFIG) & ~(RF98_PA_SELECT);
        t |= RF98_PA_SELECT;
        spi_writereg(RF98_REG_09_PA_CONFIG, t);
        eeprom_write_byte(CFG_PAC_IDX, t);
        break;
      }
    }
  } while ( (flag.tnc_mode == MODE_SETUP) && (input != '\r') );
  input = '\0';
}

static void menue_adv_paconfig_mpower_draw()
{
  uint8_t i, t;
  clearScreen();
  goHome();
  uart_println("RegPaConfig MaxPower:");
  uart_println("");
  t = ( spi_readreg(RF98_REG_09_PA_CONFIG) & RF98_MAX_POWER ) >> 4;
  for (i = 0; i <= 7; i++) {
    itoa_simple(dig, i);
    (t == i) ? uart_puts("+") : uart_puts(" ");
    uart_puts(dig);
    uart_puts(" - ");
    uart_println(dig);
  }
  uart_println("[ENTER] - Return to the previous menue");
}

static void menue_adv_paconfig_mpower()
{
  uint8_t t;

  do
  {
    menue_adv_paconfig_mpower_draw();
    __wait_for_interrupt();
    if ( flag.uart_rxne )
    {
      input = uart_getc();
      if ( (input >= '0') && (input <= '7') )
      {
        t = spi_readreg(RF98_REG_09_PA_CONFIG) & ~(RF98_MAX_POWER);
        t |= ( input - '0' ) << 4;
        spi_writereg(RF98_REG_09_PA_CONFIG, t);
        eeprom_write_byte(CFG_PAC_IDX, t);
      }
    }
  } while ( (flag.tnc_mode == MODE_SETUP) && (input != '\r') );
  input = '\0';
}

static void menue_adv_paconfig_opower_draw()
{
  uint8_t i, t;

  clearScreen();
  goHome();
  uart_println("RegPaConfig OutputPower:");
  uart_println("");
  t = spi_readreg(RF98_REG_09_PA_CONFIG) & RF98_OUTPUT_POWER;
  for (i = 0; i <= 15; i++) {
    (t == i) ? uart_puts("+") : uart_puts(" ");
    itoa_simple(dig, i);
    if ( i <=9 )
    {
      uart_puts(dig);
    } else {
      switch ( i )
      {
      case 10:
        uart_puts("a");
        break;
      case 11:
        uart_puts("b");
        break;
      case 12:
        uart_puts("c");
        break;
      case 13:
        uart_puts("d");
        break;
      case 14:
        uart_puts("e");
        break;
      case 15:
        uart_puts("f");
        break;
      }
    }
    uart_puts(" - ");
    uart_println(dig);
  }
  uart_println("[ENTER] - Return to the previous menue");
}

static void menue_adv_paconfig_opower()
{
  uint8_t t;

  do
  {
    menue_adv_paconfig_opower_draw();
    __wait_for_interrupt();
    if ( flag.uart_rxne )
    {
      input = uart_getc();
      if ( (input >= '0') && (input <= '9') || (input >= 'a') && (input <= 'f') )
      {
        t = spi_readreg(RF98_REG_09_PA_CONFIG) & ~(RF98_OUTPUT_POWER);
        if ( (input >= '0') && (input <= '9') )
          t |= ( input - '0' );
        else
          t |= ( input - 'a' + 10 );
        spi_writereg(RF98_REG_09_PA_CONFIG, t);
        eeprom_write_byte(CFG_PAC_IDX, t);
      }
    }
  } while ( (flag.tnc_mode == MODE_SETUP) && (input != '\r') );
  input = '\0';
}

static void menue_adv_paconfig()
{
  do
  {
    menue_adv_paconfig_draw();
    __wait_for_interrupt();
    if ( flag.uart_rxne )
    {
      input = uart_getc();
      switch ( input ) {
      case '1':
        menue_adv_paconfig_paselect();
        break;
      case '2':
        menue_adv_paconfig_mpower();
        break;
      case '3':
        menue_adv_paconfig_opower();
        break;
      }
    }
  } while ( (flag.tnc_mode == MODE_SETUP) && (input != '\r') );
  input = '\0';
}

static void menue_adv_paramp_draw()
{
  uint8_t i, t;

  clearScreen();
  goHome();
  uart_puts("RegPaRamp (0x0A): 0x");
  itox_simple(dig, spi_readreg(RF98_REG_0A_PA_RAMP));
  uart_puts(dig);
  uart_puts(" 0b");
  itob_simple(dig, spi_readreg(RF98_REG_0A_PA_RAMP));
  uart_println(dig);
  uart_println("");
  t = spi_readreg(RF98_REG_0A_PA_RAMP) & RF98_PA_RAMP;
  for (i = 0; i <= 15; i++) {
    (t == i) ? uart_puts("+") : uart_puts(" ");
    itoa_simple(dig, i);
    if ( i <=9 )
    {
      uart_puts(dig);
    } else {
      switch ( i )
      {
      case 10:
        uart_puts("a");
        break;
      case 11:
        uart_puts("b");
        break;
      case 12:
        uart_puts("c");
        break;
      case 13:
        uart_puts("d");
        break;
      case 14:
        uart_puts("e");
        break;
      case 15:
        uart_puts("f");
        break;
      }
    }
    uart_puts(" - ");
    uart_println(ramp_levels[i]);
  }
  uart_println("[ENTER] - Return to the previous menue");
}

static void menue_adv_paramp()
{
  uint8_t t;

  do
  {
    menue_adv_paramp_draw();
    __wait_for_interrupt();
    if ( flag.uart_rxne )
    {
      input = uart_getc();
      if ( (input >= '0') && (input <= '9') || (input >= 'a') && (input <= 'f') )
      {
        t = spi_readreg(RF98_REG_0A_PA_RAMP) & ~(RF98_PA_RAMP);
        if ( (input >= '0') && (input <= '9') )
          t |= ( input - '0' );
        else
          t |= ( input - 'a' + 10 );
        spi_writereg(RF98_REG_0A_PA_RAMP, t);
        eeprom_write_byte(CFG_PAR_IDX, t);
      }
    }
  } while ( (flag.tnc_mode == MODE_SETUP) && (input != '\r') );
  input = '\0';
}

static void menue_adv_ocp_draw()
{
  clearScreen();
  goHome();
  uart_puts("RegOcp (0x0B): 0x");
  itox_simple(dig, spi_readreg(RF98_REG_0B_OCP));
  uart_puts(dig);
  uart_puts(" 0b");
  itob_simple(dig, spi_readreg(RF98_REG_0B_OCP));
  uart_println(dig);
  uart_println("");
  uart_puts("1 - OcpOn: ");
  if ( (spi_readreg(RF98_REG_0B_OCP) & RF98_OCP_ON ) >> 5 )
    uart_println("OCP enabled");
  else
    uart_println("OCP disabled");
  uart_puts("2 - OcpTrim: ");
  itoa_simple(dig, (spi_readreg(RF98_REG_0B_OCP) & RF98_OCP_TRIM) );
  uart_println(dig);
  uart_println("[ENTER] - Return to the previous menue");
}

static void menue_adv_ocp_ocpon_draw()
{
  uint8_t t;

  clearScreen();
  goHome();
  uart_println("RegOcp OcpOn:");
  uart_println("");
  t = ( spi_readreg(RF98_REG_0B_OCP) & RF98_OCP_ON ) >> 5;
  t ? uart_puts(" ") : uart_puts("+");
  uart_println("0 - OCP disabled");
  t ? uart_puts("+") : uart_puts(" ");
  uart_println("1 - OCP enabled");
  uart_println("[ENTER] - Return to the previous menue");
}

static void menue_adv_ocp_ocpon()
{
  uint8_t t;

  do
  {
    menue_adv_ocp_ocpon_draw();
    __wait_for_interrupt();
    if ( flag.uart_rxne )
    {
      input = uart_getc();
      switch ( input ) {
      case '0':
        t = spi_readreg(RF98_REG_0B_OCP) & ~(RF98_OCP_ON);
        spi_writereg(RF98_REG_0B_OCP, t);
        eeprom_write_byte(CFG_OCP_IDX, t);
        break;
      case '1':
        t = spi_readreg(RF98_REG_0B_OCP) & ~(RF98_OCP_ON);
        t |= RF98_OCP_ON;
        spi_writereg(RF98_REG_0B_OCP, t);
        eeprom_write_byte(CFG_OCP_IDX, t);
        break;
      }
    }
  } while ( (flag.tnc_mode == MODE_SETUP) && (input != '\r') );
  input = '\0';
}

static void menue_adv_ocp_ocptrim_draw()
{
  clearScreen();
  goHome();
  uart_println("OcpTrim:");
  uart_println("");
  uart_puts("Current OcpTrim: ");
  itoa_simple(dig, spi_readreg(RF98_REG_0B_OCP) & RF98_OCP_TRIM);
  uart_println(dig);
  uart_puts("Enter OcpTrim in range 0..31 or hit [ENTER] to return: ");
}

static void menue_adv_ocp_ocptrim()
{
  uint8_t t;

  do
  {
    menue_adv_ocp_ocptrim_draw();
    uart_readln(dig, DIG_SZ);
    if ( dig[0] != '\0' )
    {
      t = spi_readreg(RF98_REG_0B_OCP) & ~(RF98_OCP_TRIM);
      t |= (uint8_t)(atoi_simple(dig) % 32);
      eeprom_write_byte(CFG_OCP_IDX, t);
      spi_writereg(RF98_REG_0B_OCP, t);
    }
  } while ( (flag.tnc_mode == MODE_SETUP) && (dig[0] != '\0') );
}

static void menue_adv_ocp()
{
  do
  {
    menue_adv_ocp_draw();
    __wait_for_interrupt();
    if ( flag.uart_rxne )
    {
      input = uart_getc();
      switch ( input ) {
      case '1':
        menue_adv_ocp_ocpon();
        break;
      case '2':
        menue_adv_ocp_ocptrim();
        break;
      }
    }
  } while ( (flag.tnc_mode == MODE_SETUP) && (input != '\r') );
  input = '\0';
}


static void menue_adv_lna_draw()
{
  clearScreen();
  goHome();
  uart_puts("RegLna (0x0C): 0x");
  itox_simple(dig, spi_readreg(RF98_REG_0C_LNA));
  uart_puts(dig);
  uart_puts(" 0b");
  itob_simple(dig, spi_readreg(RF98_REG_0C_LNA));
  uart_println(dig);
  uart_println("");
  uart_puts("1 - LnaGain: ");
  itoa_simple(dig, BFN_GET(spi_readreg(RF98_REG_0C_LNA), RF98_LNA_GAIN));
  uart_println(dig);
  uart_puts("2 - LnaBoostLf: ");
  itoa_simple(dig, BFN_GET(spi_readreg(RF98_REG_0C_LNA), RF98_LNA_BOOST_LF));
  uart_println(dig);
  uart_puts("3 - LnaBoostHf: ");
  itoa_simple(dig, BFN_GET(spi_readreg(RF98_REG_0C_LNA), RF98_LNA_BOOST_HF));
  uart_println(dig);
  uart_println("[ENTER] - Return to the previous menue");
}

static void menue_adv_lna_lnagain_draw()
{
  uint8_t i;

  clearScreen();
  goHome();
  uart_println("RegLna LnaGain:");
  uart_println("");
  for (i=0;i<LNA_SZ;i++)
  {
    if ( BFN_GET(spi_readreg(RF98_REG_0C_LNA), RF98_LNA_GAIN) == i + 1 )
      uart_putchar('+');
    else
      uart_putchar(' ');
    uart_putchar(i+'1');
    uart_puts(" - ");
    uart_println(lna_levels[i]);
  }
  uart_println(" [ENTER] - Return to the previous menue");
}

static void menue_adv_lna_lnagain()
{
  uint8_t t;

  do
  {
    menue_adv_lna_lnagain_draw();
    __wait_for_interrupt();
    if ( flag.uart_rxne )
    {
      input = uart_getc();
      if ( (input >= '1') && (input <= '6') )
      {
        t = spi_readreg(RF98_REG_0C_LNA) & ~(RF98_LNA_GAIN_MASK);
        switch ( input )
        {
        case '1':
          t |= RF98_LNA_GAIN_G1;
          break;
        case '2':
          t |= RF98_LNA_GAIN_G2;
          break;
        case '3':
          t |= RF98_LNA_GAIN_G3;
          break;
        case '4':
          t |= RF98_LNA_GAIN_G4;
          break;
        case '5':
          t |= RF98_LNA_GAIN_G5;
          break;
        case '6':
          t |= RF98_LNA_GAIN_G6;
          break;
        }
        spi_writereg(RF98_REG_0C_LNA, t);
        eeprom_write_byte(CFG_LNA_IDX, t);
      }
    }
  } while ( (flag.tnc_mode == MODE_SETUP) && (input != '\r') );
  input = '\0';
}

static void menue_adv_lna_lnabhf_draw()
{
  uint8_t t;

  clearScreen();
  goHome();
  uart_println("RegLna LnaBoostHf:");
  uart_println("");
  t = spi_readreg(RF98_REG_0C_LNA) & RF98_LNA_BOOST_HF_MASK;
  t == RF98_LNA_BOOST_HF_DEFAULT ? uart_puts("+") : uart_puts(" ");
  uart_println("0 - Default LNA current");
  t == RF98_LNA_BOOST_HF_150PC ? uart_puts("+") : uart_puts(" ");
  uart_println("3 - Boost on, 150% LNA current");
  uart_println(" [ENTER] - Return to the previous menue");
}

static void menue_adv_lna_lnabhf()
{
  uint8_t t;

  do
  {
    menue_adv_lna_lnabhf_draw();
    __wait_for_interrupt();
    if ( flag.uart_rxne )
    {
      input = uart_getc();
      t = spi_readreg(RF98_REG_0C_LNA) & ~(RF98_LNA_BOOST_HF_MASK);
      switch ( input )
      {
      case '0':
        t |= RF98_LNA_BOOST_HF_DEFAULT;
        spi_writereg(RF98_REG_0C_LNA, t);
        eeprom_write_byte(CFG_LNA_IDX, t);
        break;
      case '3':
        t |= RF98_LNA_BOOST_HF_150PC;
        spi_writereg(RF98_REG_0C_LNA, t);
        eeprom_write_byte(CFG_LNA_IDX, t);
        break;
      }
    }
  } while ( (flag.tnc_mode == MODE_SETUP) && (input != '\r') );
  input = '\0';
}

static void menue_adv_lna()
{
  do
  {
    menue_adv_lna_draw();
    __wait_for_interrupt();
    if ( flag.uart_rxne )
      input = uart_getc();
    switch ( input ) {
    case '1':
      menue_adv_lna_lnagain();
      break;
    case '2':
      //menue_adv_lna_lnablf();
      break;
    case '3':
      menue_adv_lna_lnabhf();
      break;
    }
  } while ( (flag.tnc_mode == MODE_SETUP) && (input != '\r') );
  input = '\0';
}

static void menue_adv_preamble_draw()
{
  uint16_t t;

  clearScreen();
  goHome();
  uart_println("Preamble:");
  uart_puts("   RegPreambleMsb (0x20): 0x");
  itox_simple(dig, spi_readreg(RF98_REG_20_PREAMBLE_MSB));
  uart_puts(dig);
  uart_puts(" 0b");
  itob_simple(dig, spi_readreg(RF98_REG_20_PREAMBLE_MSB));
  uart_println(dig);
  uart_puts("   RegPreambleLsb (0x21): 0x");
  itox_simple(dig, spi_readreg(RF98_REG_21_PREAMBLE_LSB));
  uart_puts(dig);
  uart_puts(" 0b");
  itob_simple(dig, spi_readreg(RF98_REG_21_PREAMBLE_LSB));
  uart_println(dig);
  uart_println("");
  uart_puts("Current Preamble: ");
  t = spi_readreg(RF98_REG_20_PREAMBLE_MSB) << 8;
  t |= spi_readreg(RF98_REG_21_PREAMBLE_LSB);
  itoa_simple(dig, t);
  uart_println(dig);
  uart_puts("Enter Preamble in range 0..65535 or hit [ENTER] to return: ");
}

static void menue_adv_preamble()
{
  uint8_t t;
  uint16_t m;

  do
  {
    menue_adv_preamble_draw();
    uart_readln(dig, DIG_SZ);
    if ( dig[0] != '\0' )
    {
      m = (uint16_t)atoi_simple(dig);
      t = HIBYTE(m);
      spi_writereg(RF98_REG_20_PREAMBLE_MSB, t);
      eeprom_write_byte(CFG_PRU_IDX, t);
      t = LOBYTE(m);
      spi_writereg(RF98_REG_21_PREAMBLE_LSB, t);
      eeprom_write_byte(CFG_PRL_IDX, t);
    }
  } while ( (flag.tnc_mode == MODE_SETUP) && (dig[0] != '\0') );
}

static void menue_adv_lowdropt_draw()
{
  uint8_t t;

  clearScreen();
  goHome();
  uart_println("LowDatarateOptimize:");
  uart_puts("   RegModemConfig3 (0x26): 0x");
  itox_simple(dig, spi_readreg(RF98_REG_26_MODEM_CONFIG3));
  uart_puts(dig);
  uart_puts(" 0b");
  itob_simple(dig, spi_readreg(RF98_REG_26_MODEM_CONFIG3));
  uart_println(dig);
  uart_println("");
  t = ( spi_readreg(RF98_REG_26_MODEM_CONFIG3) & RF98_LDOPTIMIZE ) >> 3;
  t ? uart_puts(" ") : uart_puts("+");
  uart_println("0 - Disabled");
  t ? uart_puts("+") : uart_puts(" ");
  uart_println("1 - Enabled; mandated for when the symbol length exceeds 16ms");
  uart_println("[ENTER] - Return to the previous menue");
}

static void menue_adv_lowdropt()
{
  uint8_t t;

  do
  {
    menue_adv_lowdropt_draw();
    __wait_for_interrupt();
    if ( flag.uart_rxne )
    {
      input = uart_getc();
      switch ( input ) {
      case '0':
        t = spi_readreg(RF98_REG_26_MODEM_CONFIG3) & ~(RF98_LDOPTIMIZE);
        spi_writereg(RF98_REG_26_MODEM_CONFIG3, t);
        eeprom_write_byte(CFG_MC3_IDX, t);
        break;
      case '1':
        t = spi_readreg(RF98_REG_26_MODEM_CONFIG3) & ~(RF98_LDOPTIMIZE);
        t |= RF98_LDOPTIMIZE;
        spi_writereg(RF98_REG_26_MODEM_CONFIG3, t);
        eeprom_write_byte(CFG_MC3_IDX, t);
        break;
      }
    }
  } while ( (flag.tnc_mode == MODE_SETUP) && (input != '\r') );
  input = '\0';
}

static void menue_adv_agcaon_draw()
{
  uint8_t t;

  clearScreen();
  goHome();
  uart_println("AgcAutoOn:");
  uart_puts("   RegModemConfig3 (0x26): 0x");
  itox_simple(dig, spi_readreg(RF98_REG_26_MODEM_CONFIG3));
  uart_puts(dig);
  uart_puts(" 0b");
  itob_simple(dig, spi_readreg(RF98_REG_26_MODEM_CONFIG3));
  uart_println(dig);
  uart_println("");
  t = ( spi_readreg(RF98_REG_26_MODEM_CONFIG3) & RF98_AGCAUTOON ) >> 2;
  t ? uart_puts(" ") : uart_puts("+");
  uart_println("0 - Disabled. LNA gain set by register LnaGain");
  t ? uart_puts("+") : uart_puts(" ");
  uart_println("1 - Enabled. LNA gain set by the internal AGC loop");
  uart_println("[ENTER] - Return to the previous menue");
}

static void menue_adv_agcaon()
{
  uint8_t t;

  do
  {
    menue_adv_agcaon_draw();
    __wait_for_interrupt();
    if ( flag.uart_rxne )
    {
      input = uart_getc();
      switch ( input ) {
      case '0':
        t = spi_readreg(RF98_REG_26_MODEM_CONFIG3) & ~(RF98_AGCAUTOON);
        spi_writereg(RF98_REG_26_MODEM_CONFIG3, t);
        eeprom_write_byte(CFG_MC3_IDX, t);
        break;
      case '1':
        t = spi_readreg(RF98_REG_26_MODEM_CONFIG3) & ~(RF98_AGCAUTOON);
        t |= RF98_AGCAUTOON;
        spi_writereg(RF98_REG_26_MODEM_CONFIG3, t);
        eeprom_write_byte(CFG_MC3_IDX, t);
        break;
      }
    }
  } while ( (flag.tnc_mode == MODE_SETUP) && (input != '\r') );
  input = '\0';
}

static void menue_adv_ppmcorr_draw()
{
  clearScreen();
  goHome();
  uart_println("RegPpmCorrection (0x27):");
  uart_println("");
  uart_puts("Current PpmCorrection: ");
  itoa_simple(dig, spi_readreg(RF98_REG_27_PPM_CORRECTION));
  uart_println(dig);
  uart_puts("Enter RegPpmCorrection in range 0..255 or hit [ENTER] to return: ");
}

static void menue_adv_ppmcorr()
{
  uint8_t t;

  do
  {
    menue_adv_ppmcorr_draw();
    uart_readln(dig, DIG_SZ);
    if ( dig[0] != '\0' )
    {
      t = (uint8_t)atoi_simple(dig);
      spi_writereg(RF98_REG_27_PPM_CORRECTION, t);
      eeprom_write_byte(CFG_PPM_IDX, t);
    }
  } while ( (flag.tnc_mode == MODE_SETUP) && (dig[0] != '\0') );
}

static void menue_adv_detectopt_draw()
{
  uint8_t t;

  clearScreen();
  goHome();
  uart_puts("RegDetectOptimize (0x31): 0x");
  itox_simple(dig, spi_readreg(RF98_REG_31_DETECT_OPTIMIZ));
  uart_puts(dig);
  uart_puts(" 0b");
  itob_simple(dig, spi_readreg(RF98_REG_31_DETECT_OPTIMIZ));
  uart_println(dig);
  uart_println("");
  t = spi_readreg(RF98_REG_31_DETECT_OPTIMIZ) & RF98_DETOPTIMIZE;
  (t == RF98_DETOPTIMIZE_03) ? uart_puts("+") : uart_puts(" ");
  uart_println("0 - 0x03 -> SF7 to SF12");
  (t == RF98_DETOPTIMIZE_05) ? uart_puts("+") : uart_puts(" ");
  uart_println("1 - 0x05 -> SF6");
  uart_println("[ENTER] - Return to the previous menue");
}

static void menue_adv_detectopt()
{
  uint8_t t;

  do
  {
    menue_adv_detectopt_draw();
    __wait_for_interrupt();
    if ( flag.uart_rxne )
    {
      input = uart_getc();
      switch ( input ) {
      case '0':
        t = spi_readreg(RF98_REG_31_DETECT_OPTIMIZ) & ~(RF98_DETOPTIMIZE);
        t |= RF98_DETOPTIMIZE_03;
        spi_writereg(RF98_REG_31_DETECT_OPTIMIZ, t);
        eeprom_write_byte(CFG_DEO_IDX, t);
        break;
      case '1':
        t = spi_readreg(RF98_REG_31_DETECT_OPTIMIZ) & ~(RF98_DETOPTIMIZE);
        t |= RF98_DETOPTIMIZE_05;
        spi_writereg(RF98_REG_31_DETECT_OPTIMIZ, t);
        eeprom_write_byte(CFG_DEO_IDX, t);
        break;
      }
    }
  } while ( (flag.tnc_mode == MODE_SETUP) && (input != '\r') );
  input = '\0';
}

static void menue_adv_invertiq_draw()
{
  uint8_t t;

  clearScreen();
  goHome();
  uart_puts("RegInvertIQ (0x33): 0x");
  itox_simple(dig, spi_readreg(RF98_REG_33_INVERT_IQ));
  uart_puts(dig);
  uart_puts(" 0b");
  itob_simple(dig, spi_readreg(RF98_REG_33_INVERT_IQ));
  uart_println(dig);
  uart_println("");
  t = spi_readreg(RF98_REG_33_INVERT_IQ) & RF98_INVERT_IQ;
  t ? uart_puts(" ") : uart_puts("+");
  uart_println("0 - normal mode");
  t ? uart_puts("+") : uart_puts(" ");
  uart_println("1 - I and Q signals are inverted");
  uart_println("[ENTER] - Return to the previous menue");
}

static void menue_adv_invertiq()
{
  uint8_t t;

  do
  {
    menue_adv_invertiq_draw();
    __wait_for_interrupt();
    if ( flag.uart_rxne )
    {
      input = uart_getc();
      switch ( input ) {
      case '0':
        t = spi_readreg(RF98_REG_33_INVERT_IQ) & ~(RF98_INVERT_IQ);
        spi_writereg(RF98_REG_33_INVERT_IQ, t);
        eeprom_write_byte(CFG_IIQ_IDX, t);
        break;
      case '1':
        t = spi_readreg(RF98_REG_33_INVERT_IQ) & ~(RF98_INVERT_IQ);
        t |= RF98_INVERT_IQ;
        spi_writereg(RF98_REG_33_INVERT_IQ, t);
        eeprom_write_byte(CFG_IIQ_IDX, t);
        break;
      }
    }
  } while ( (flag.tnc_mode == MODE_SETUP) && (input != '\r') );
  input = '\0';
}

static void menue_adv_detth_draw()
{
  uint8_t t;

  clearScreen();
  goHome();
  uart_puts("RegDetectionThreshold (0x37): 0x");
  itox_simple(dig, spi_readreg(RF98_REG_37_DETECTION_THRESHOLD));
  uart_puts(dig);
  uart_puts(" 0b");
  itob_simple(dig, spi_readreg(RF98_REG_37_DETECTION_THRESHOLD));
  uart_println(dig);
  uart_println("");
  t = spi_readreg(RF98_REG_37_DETECTION_THRESHOLD);
  (t == RF98_DETTH_0A) ? uart_puts("+") : uart_puts(" ");
  uart_println("0 - 0x0A -> SF7 to SF12");
  (t == RF98_DETTH_0C) ? uart_puts("+") : uart_puts(" ");
  uart_println("1 - 0x0C -> SF6");
  uart_println("[ENTER] - Return to the previous menue");
}

static void menue_adv_detth()
{
  do
  {
    menue_adv_detth_draw();
    __wait_for_interrupt();
    if ( flag.uart_rxne )
    {
      input = uart_getc();
      switch ( input ) {
      case '0':
        spi_writereg(RF98_REG_37_DETECTION_THRESHOLD, RF98_DETTH_0A);
        eeprom_write_byte(CFG_DTT_IDX, RF98_DETTH_0A);
        break;
      case '1':
        spi_writereg(RF98_REG_37_DETECTION_THRESHOLD, RF98_DETTH_0C);
        eeprom_write_byte(CFG_DTT_IDX, RF98_DETTH_0C);
        break;
      }
    }
  } while ( (flag.tnc_mode == MODE_SETUP) && (input != '\r') );
  input = '\0';
}

static void menue_adv_syncword_draw()
{
  clearScreen();
  goHome();
  uart_println("RegSyncWord (0x39):");
  uart_println("");
  uart_puts("Current SyncWord (decimal): ");
  itoa_simple(dig, spi_readreg(RF98_REG_39_SYNC_WORD));
  uart_println(dig);
  uart_puts("Enter SyncWord in range 0..255 or hit [ENTER] to return: ");
}

static void menue_adv_syncword()
{
  uint8_t t;

  do
  {
    menue_adv_syncword_draw();
    uart_readln(dig, DIG_SZ);
    if ( dig[0] != '\0' )
    {
      t = (uint8_t)atoi_simple(dig);
      spi_writereg(RF98_REG_39_SYNC_WORD, t);
      eeprom_write_byte(CFG_SCW_IDX, t);
    }
  } while ( (flag.tnc_mode == MODE_SETUP) && (dig[0] != '\0') );
}

static void menue_adv()
{
  do
  {
    menue_adv_draw();
    __wait_for_interrupt();
    if ( flag.uart_rxne )
      input = uart_getc();
    switch ( input ) {
    case '0':
      menue_adv_paconfig();
      break;
    case '1':
      menue_adv_paramp();
      break;
    case '2':
      menue_adv_ocp();
      break;
    case '3':
      menue_adv_lna();
      break;
    case '4':
      menue_adv_preamble();
      break;
    case '5':
      menue_adv_lowdropt();
      break;
    case '6':
      menue_adv_agcaon();
      break;
    case '7':
      menue_adv_ppmcorr();
      break;
    case '8':
      menue_adv_detectopt();
      break;
    case '9':
      menue_adv_invertiq();
      break;
    case 'a':
      menue_adv_detth();
      break;
    case 'b':
      menue_adv_syncword();
      break;
    }
  } while ( (flag.tnc_mode == MODE_SETUP) && (input != '\r') );
  input = '\0';
}

static void menue_regdump()
{
  uint8_t n, t;

  clearScreen();
  goHome();

  for (uint8_t i=0; i <= 15; i++)
  {
    for (uint8_t j=0;j<=3;j++)
    {
      n = j+i*4;
      itox_simple(dig, n);
      uart_puts(dig);
      uart_puts(": ");
      t = spi_readreg(n);
      itox_simple(dig, t);
      uart_puts(dig);
      uart_putchar(' ');
      itob_simple(dig, t);
      uart_puts(dig);
      uart_puts(" | ");
    }
    uart_puts("\r\n");
  }
  uart_puts("\r\n");
  uart_puts("Press any key to continue");
  __wait_for_interrupt();
}

static void menue_draw()
{
  clearScreen();
  goHome();
  uart_println("KISS TNC for SX127x or RF(M)9x LoRa modems");
  uart_println("");
  uart_puts("Onboard Semtech chip id: 0x");
  itox_simple(dig, spi_readreg(RF98_REG_42_VERSION));
  uart_println(dig);
  uart_println("");
  uart_println("Select TNC settings:");
  uart_println("");
  uart_println("1 - Basic settings");
  uart_println("2 - Advanced settings");
  uart_println("3 - Initialise TNC EEPROM");
  uart_println("4 - Dump SX127x hardware registers");
}

void menue()
{
  rf_setopmode(RF98_MODE_STDBY);
  tim2_stop();
  uart_stop();
  uart_init(BD9600);
  input = '\0';
  do 
  {
    menue_draw();
    __wait_for_interrupt();
    if ( flag.uart_rxne ) {
      input = uart_getc();
      switch ( input ) {
      case '1':
        menue_basic();
        break;
      case '2':
        menue_adv();
        break;
      case '3':
        menue_readmodem();
        break;
      case '4':
        menue_regdump();
        break;
      }
    }
  } while (flag.tnc_mode == MODE_SETUP);
}
