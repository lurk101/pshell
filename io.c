
#if LIB_PICO_STDIO_UART
	#include "pico/stdio_uart.h"
	#include "hardware/uart.h"
#endif
#if LIB_PICO_STDIO_USB
	#include "pico/stdio_usb.h"
#endif

#include "io.h"

int ioinit(void) {
#if LIB_PICO_STDIO_UART
	stdio_uart_init();
	uart_set_fifo_enabled(uart0, 1);
#endif
#if LIB_PICO_STDIO_USB
	stdio_usb_init();
#endif
}

static int unconsumed = PICO_ERROR_TIMEOUT;

int x_getchar(void) {
    if (unconsumed != PICO_ERROR_TIMEOUT) {
        int c = unconsumed;
        unconsumed = PICO_ERROR_TIMEOUT;
        return c;
    }
    return getchar();
}

int x_getchar_timeout_us(unsigned t) {
    if (unconsumed != PICO_ERROR_TIMEOUT) {
        int c = unconsumed;
        unconsumed = PICO_ERROR_TIMEOUT;
        return c;
    }
    return getchar_timeout_us(t);
}

void set_translate_crlf(bool on) {
#if LIB_PICO_STDIO_UART
    stdio_set_translate_crlf(&stdio_uart, on);
#endif
#if LIB_PICO_STDIO_USB
    stdio_set_translate_crlf(&stdio_usb, on);
#endif
}

int nextchar(void) { return unconsumed; }
