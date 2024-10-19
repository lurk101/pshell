/* SDK GPIO interface test. Blink the default LED. */

int main() {
    int led_pin = PICO_DEFAULT_LED_PIN;
    gpio_init(led_pin);
    gpio_set_dir(led_pin, GPIO_OUT);
    int tic = 0;
    for (;;) {
        gpio_put(led_pin, tic ^= 1);
        if (getchar_timeout_us(500000) == 3)
            break;
    }
    gpio_put(led_pin, 0);
    return 0;
}
