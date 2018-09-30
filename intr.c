#include "config.h"
#include "uart.h"
#include "timer.h"
#include "kiss.h"

#pragma vector=USART_R_RXNE_vector
__interrupt void USART_R_RXNE_handler(void) {
  if (USART1_SR & MASK_USART1_SR_RXNE) {
    flag.uart_rxne = true;
    uart_dr = USART1_DR;
    /* Hardware flow control */
    if ( CBUF_Len(utmpbuf) >= TTBH_SZ )
    {
      RTS_SET_HIGH;
    }
    CBUF_Push(utmpbuf, uart_dr);
  }
  else {
    USART1_DR; // implicit read
  }
  flag.uart_timer_restarted = true;
}

#pragma vector=EXTI5_vector // Pin 5 interrupts, see datasheet
__interrupt void EXTI5_handler(void)
{
  /* M0 Switch handler */
  EXTI_SR1_P5F = 1;     // Clear the interrupt flag
  if ( M0_SW == HIGH )
    __reset();
  else
    flag.tnc_mode = MODE_SETUP;
}

#pragma vector=EXTI6_vector // Pin 6 interrupts, see datasheet
__interrupt void EXTI6_handler(void)
{
  /* M1 Switch handler */
  __reset();
}

#pragma vector=TIM1_OVR_UIF_vector
__interrupt void TIM1_OVR_UIF_handler(void)
{
  TIM1_SR1_UIF = 0;         //clear interrupt flag
  flag.tfc_timeout = true;
  tim1_stop();
}

#pragma vector=TIM2_OVR_UIF_vector
__interrupt void TIM2_OVR_UIF_handler(void)
{
  TIM2_SR1_UIF = 0;         //clear interrupt flag

  if (tim2_cnt)
  {
    tim2_cnt--;
  }

  if ( flag.uart_timer_pending && !flag.uart_timer_restarted )
  {
    flag.uart_timer_pending = false;
    flag.uart_rx_complete = true;
  }

  if ( flag.uart_timer_restarted )
  {
    flag.uart_timer_restarted = false;
    flag.uart_timer_pending = true;
  }
}

#pragma vector=TIM3_OVR_UIF_vector
__interrupt void TIM3_OVR_UIF_handler(void)
{
  TIM3_SR1_UIF = 0;         //clear interrupt flag
  flag.t1s_reset = false;
}
