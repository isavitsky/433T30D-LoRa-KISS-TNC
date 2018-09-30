#include "config.h"
#include "uart.h"

char uart_dr;

void uart_init(uint8_t baudspec)
{
  uint32_t baud;

  // Configure PA3 (USART1_RX) for input
  PA_DDR_DDR3 = 0;      // in
  PA_CR1_C13 = 0;       // float
  PA_CR2_C23 = 0;       // no extint

  // Configure PA2 (USART1_TX) for output
  PA_DDR_DDR2 = 1; // out
  PA_CR1_C12 = 1; // pp
  PA_CR2_C22 = 1; // fast
  PA_ODR_ODR2 = 1; // high

  CLK_PCKENR1_PCKEN15 = 1;      // Enable clock to USART1
  
  SYSCFG_RMPCR1_USART1TR_REMAP = 0x01; // Remap TX to PA2, RX to PA3

  // Set speed and 8,N,1 mode
  USART1_CR1_M = 0;     // 8 bits


  switch ( baudspec )
  {
  case BD9600:
    baud = 9600;
    break;
  case BD19200:
    baud = 19200;
    break;
  case BD38400:
    baud = 38400;
    break;
  case BD57600:
    baud = 57600;
    break;
  case BD115200:
    baud = 115200;
    break;
  case BD230400:
    baud = 230400;
    break;
  case BD460800:
    baud = 460800;
    break;
  case BD921600:
    baud = 921600;
    break;
  default:
    baud = 9600;
  }

  USART1_BRR2 = (char)(((UART_DIV(baud) & 0xF000)>> 8) + (UART_DIV(baud) & 0x000F));  // set
  USART1_BRR1 = (char)((UART_DIV(baud) & 0x0FF0) >> 4);                               // baudrate

  USART1_CR2_TEN = 1;   // TX enable
  USART1_CR2_REN = 1;   // RX enable
  USART1_CR2_RIEN = 1;  // Enable RX interrupt
}

void uart_stop(void)
{
  while (USART1_SR_TC == 0); // disable only after tx complete
  USART1_CR2_TEN = 0;   // TX disable
  USART1_CR2_REN = 0;   // RX disable
  USART1_CR2_RIEN = 0;  // Disable RX interrupt
  CLK_PCKENR1_PCKEN15 = 0;
  
  // Configure PD6 (USART1_RX) for output
  PA_DDR_DDR3 = 1;      // out
  PA_CR1_C13 = 0;       // open drain PA3

  PA_CR1_C12 = 0;       // open drain PA2
}

void uart_putchar(char c)
{
  while (USART1_SR_TXE == 0);
  USART1_DR = c;
}

char uart_getchar(void)
{
  while(USART1_SR_RXNE == 0); // Wait until char is available
  return (USART1_DR);       // Get it
}

char uart_getc(void)
{
  flag.uart_rxne = false;
  return uart_dr;
}

void uart_puts(const char *s)
{
    while (s && *s)
      uart_putchar (*s++);
}

void uart_putsn(const char *s, uint16_t len)
{
  if ( len == 0 ) return;
  while (len--)
    uart_putchar (*s++);
}

void uart_println(const char *s)
{
    while (s && *s)
      uart_putchar (*s++);
    uart_puts("\r\n");
}

void uart_readln(char *buf, uint8_t sz)
{
  char input;
  uint8_t n = 0;

  do
  {
    __wait_for_interrupt();
    if ( flag.uart_rxne )
    {
      input = uart_getc();
      uart_putchar(input);
      buf[n++] = input;
    }
  } while (input != '\r' && n < sz);
  if ( n < sz )
    buf[n-1] = '\0';
  else
    buf[sz-1] = '\0';
}