/* SDK GPIO interface test. Blink the default LED. */

int tic, slice, pin;

int on_pwm_wrap() {
    pwm_clear_irq(slice);
    gpio_put(pin, tic ^= 1);
}

int main() {
    struct {
        int csr;
        int div;
        int top;
    } pwm_config;
    tic = 0;
    pin = PICO_DEFAULT_LED_PIN;
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_OUT);
    slice = pwm_gpio_to_slice_num(pin);
    pwm_clear_irq(slice);
    pwm_set_irq_enabled(slice, TRUE);
    memcpy(&pwm_config, pwm_get_default_config(), sizeof(pwm_config)); // can't assign structs
    pwm_config_set_clkdiv(&pwm_config, 255.0);
    irq_set_exclusive_handler(PWM_IRQ_WRAP, on_pwm_wrap);
    pwm_init(slice, &pwm_config, TRUE);
    irq_set_enabled(PWM_IRQ_WRAP, TRUE);
    while (1)
        wfi();
}
