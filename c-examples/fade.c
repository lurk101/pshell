int fade, slice, going_up, led_pin;

void on_pwm_wrap() {
    pwm_clear_irq(slice);
    if (going_up) {
        if (++fade > 255) {
            fade = 255;
            going_up = false;
        }
    } else {
        if (--fade < 0) {
            fade = 0;
            going_up = true;
        }
    }
    pwm_set_gpio_level(led_pin, fade * fade);
}

int main() {
    fade = 0;
    going_up = 0;
    led_pin = PICO_DEFAULT_LED_PIN;
    gpio_set_function(led_pin, GPIO_FUNC_PWM);
    slice = pwm_gpio_to_slice_num(led_pin);
    pwm_clear_irq(slice);
    pwm_set_irq_enabled(PWM_IRQ_WRAP, true);
    irq_set_exclusive_handler(PWM_IRQ_WRAP, on_pwm_wrap);
    irq_set_enabled(PWM_IRQ_WRAP, true);
    struct {
        int csr;
        int div;
        int top;
    } config;
    memcpy((int)&config, (int)pwm_get_default_config(), sizeof(config));
    pwm_config_set_clkdiv((int)&config, 4.0);
    pwm_init(slice, (int)&config, true);
    while (1)
        wfi();
    return 0;
}
