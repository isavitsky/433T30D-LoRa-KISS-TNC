// uart.h
#ifndef UART_H
#define UART_H

#include "config.h"

#define BD9600          0
#define BD19200         1
#define BD38400         2
#define BD57600         3
#define BD115200        4
#define BD230400        5
#define BD460800        6
#define BD921600        7

#define UART_DIV(x) (int)((( F_MASTER * 1000000.0f ) / ( x ) ) + 0.5f )

extern char uart_dr;

void uart_init(uint8_t baudspec);
void uart_stop(void);
void uart_putchar(char);
void uart_puts(const char *);
void uart_putsn(const char *, uint16_t);
void uart_println(const char *);
void uart_readln(char *, uint8_t);
char uart_getchar(void);
char uart_getc(void);

#endif
