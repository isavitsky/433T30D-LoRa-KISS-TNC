// util.h
#ifndef UTIL_H
#define UTIL_H

uint8_t random(void);
uint8_t assemble(uint8_t len, uint8_t id);
char cbuf_pop2(void);
char cbuf_pop(void);
void cbuf_push(char c);
void eeprom_write_byte(uint8_t idx, uint8_t val);

#endif