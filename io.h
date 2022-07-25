#ifndef __UART_H_
#define __UART_H_

#include <stdbool.h>
#include <stdio.h>

int ioinit(void);

void set_translate_crlf(bool on);

int nextchar(void);

int x_getchar();

int x_getchar_timeout_us(unsigned t);

#endif
