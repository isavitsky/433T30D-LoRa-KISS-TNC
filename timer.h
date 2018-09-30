// timer.h
#ifndef TIMER_H
#define TIMER_H

#define LOG_BASE_2(x) ((x) <= 1 ? 0 : (x) <= 2 ? 1 : (x) <= 4 ? 2 : (x) <= 8 ? 3 : (x) <= 16 ? 4 : (x) <= 32 ? 5 : (x) <= 64 ? 6 : (x) <= 128 ? 7 : (x) <= 256 ? 8 : \
  (x) <= 512 ? 9 : (x) <= 1024 ? 10 : (x) <= 2048 ? 11 : (x) <= 4096 ? 12 : (x) <= 8192 ? 13 : (x) <= 16384 ? 14 : 15)

#define TIM_2_PSCR(x) LOG_BASE_2(((((x) * 1000.0) * F_MASTER) / 65536.0) + 1.0)
#define TIM_2_ARR(x) (((x) * 1000.0) * F_MASTER / (1 << TIM_2_PSCR((x))))

extern uint8_t tim2_cnt;

void tim1_start(void);
void tim1_stop(void);
void tim1_restart(void);
void tim2_start(void);
void tim2_stop(void);
void delay_10ms(uint8_t count);
void tim2_restart(uint8_t count);
void adaptive_sleep(void);

#endif
