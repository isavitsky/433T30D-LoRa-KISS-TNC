#include "config.h"
#include "timer.h"

uint8_t tim2_cnt;

void tim1_start()
{
  if ( CLK_PCKENR2_PCKEN21 == 0 )
  {
    CLK_PCKENR2_PCKEN21 = 1;       // clock to TIM1
    TIM1_CR1_ARPE = 0;
    TIM1_SR1_UIF = 0;
    TIM1_PSCRH = 0x39; // 60s interval
    TIM1_PSCRL = 0x3A;
    TIM1_ARRH = 255;
    TIM1_ARRL = 249;
    TIM1_CNTRH=0;
    TIM1_CNTRL=1;
    TIM1_CR1_URS = 1;
    TIM1_IER_UIE = 1;
    TIM1_EGR_UG = 1;
    TIM1_CR1_CEN = 1;
  }
  flag.tfc_timeout = false;
}

void tim1_stop()
{
  TIM1_CR1_CEN = 0;
  TIM1_IER_UIE = 0;
  TIM1_SR1_UIF = 0; // clear interrupt
  CLK_PCKENR2_PCKEN21 = 0;
}

void tim1_restart()
{
  TIM1_CR1_CEN=0;
  TIM1_CNTRH=0;
  TIM1_CNTRL=1;
  TIM1_CR1_CEN=1;
  flag.tfc_timeout = false;
}

void tim2_start()
{
  CLK_PCKENR1_PCKEN10 = 1;       // clock to TIM2
  TIM2_CR1_ARPE = 0;
  TIM2_SR1_UIF = 0;
  TIM2_PSCR = 6; // 10 ms
  TIM2_ARRH = 9;//HIBYTE(t);
  TIM2_ARRL = 196;//LOBYTE(t);
  TIM2_IER_UIE = 1;
  TIM2_EGR_UG = 1;
  TIM2_CR1_CEN = 1;
}

void tim2_stop()
{
  TIM2_CR1_CEN = 0;
  CLK_PCKENR1_PCKEN10 = 0;       // TIM2 clock
}
  
void delay_10ms(uint8_t count)
{
  if ( count )
  {
    TIM2_CR1_CEN=0;
    TIM2_CNTRH=0;
    TIM2_CNTRL=1;
    //TIM2_EGR_UG=1;
    tim2_cnt=count;
    TIM2_CR1_CEN=1;
    while ( tim2_cnt )
      __wait_for_interrupt();
  }
}

void tim2_restart(uint8_t count)
{
  TIM2_CR1_CEN=0;
  TIM2_CNTRH=0;
  TIM2_CNTRL=1;
  tim2_cnt=count;
  TIM2_CR1_CEN=1;
}

void tim3_start()
{
  CLK_PCKENR1_PCKEN11 = 1;       // clock to TIM3
  TIM3_CR1_ARPE = 0;
  TIM3_SR1_UIF = 0;
  TIM3_PSCR = 7; // 0.5s interval
  TIM3_ARRH = 244;
  TIM3_ARRL = 36;
  TIM3_CR1_URS = 1;
  TIM3_IER_UIE = 1;
  TIM3_EGR_UG = 1;
  TIM3_CR1_CEN = 1;
  flag.t1s_reset = true;
}

void tim3_stop()
{
  TIM3_CR1_CEN = 0;
  TIM3_SR1_UIF = 0; // clear interrupt
  TIM3_IER_UIE = 0;
  CLK_PCKENR1_PCKEN11 = 0;
}

static void delay_05s()
{
  while ( flag.t1s_reset )
    __wait_for_interrupt();
}

void adaptive_sleep()
{
  // sleep for 0.5 s only if there is no traffic
  if ( flag.tfc_timeout )
  {
    tim3_start();
    delay_05s();
    tim3_stop();
  } else
  {
    tim1_start();
  }
}
