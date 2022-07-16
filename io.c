
#if LIB_PICO_STDIO_UART
	#include "pico/stdio_uart.h"
#endif
#if LIB_PICO_STDIO_USB
	#include "pico/stdio_usb.h"
#endif

#include "pico/stdio.h"
#include "pico/stdlib.h"

#include "io.h"


#if PICO_SDK_VERSION_MAJOR > 1 || (PICO_SDK_VERSION_MAJOR == 1 && PICO_SDK_VERSION_MINOR >= 4)
#define SDK14 1
#else
#define SDK14 0
#endif

#if LIB_PICO_STDIO_USB
#if SDK14
#define CONS_CONNECTED stdio_usb_connected()
#else
#include "tusb.h"
#define CONS_CONNECTED tud_cdc_connected()
#endif
#endif

int ioinit(void) {
    stdio_init_all();
#if LIB_PICO_STDIO_USB
    while (!CONS_CONNECTED)
        sleep_ms(1000);
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
