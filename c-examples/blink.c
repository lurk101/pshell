int main() {
    int LED_PIN = 25;
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, 1);
    int tic = 0;
    while (1) {
        gpio_put(LED_PIN, tic ^= 1);
        sleep_ms(500);
    }
}
