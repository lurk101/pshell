/* SDK GPIO interface test. Blink the default LED. */

int main() {
    int LED_PIN = PICO_DEFAULT_LED_PIN;
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    int tic = 0;
    while (1) {
        gpio_put(LED_PIN, tic ^= 1);
        sleep_ms(500);
    }
    return 0;
}
