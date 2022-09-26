/* SDK GPIO interface test. Blink the default LED. */

int main() {
    int led_pin = PICO_DEFAULT_LED_PIN;
    gpio_init(led_pin);
    gpio_set_dir(led_pin, GPIO_OUT);
    int tic = 0;
    while (1) {
        gpio_put(led_pin, tic ^= 1);
        if (getchar_timeout_us(0) == 3)
            break;
        sleep_ms(500);
    }
    return 0;
}
