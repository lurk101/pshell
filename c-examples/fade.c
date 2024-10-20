int fade, slice, going_up;

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
    pwm_set_gpio_level(PICO_DEFAULT_LED_PIN, fade * fade);
}

int main() {
    fade = 0;
    going_up = 0;
    gpio_set_function(PICO_DEFAULT_LED_PIN, GPIO_FUNC_PWM);
    slice = pwm_gpio_to_slice_num(PICO_DEFAULT_LED_PIN);
    pwm_clear_irq(slice);
    pwm_set_irq_enabled(slice, true);
    irq_set_exclusive_handler(PWM_IRQ_WRAP_0, on_pwm_wrap);
    irq_set_enabled(PWM_IRQ_WRAP_0, true);
    struct {
        int csr, div, top;
    } config;
    memcpy((int)&config, (int)pwm_get_default_config(), sizeof(config));
    pwm_config_set_clkdiv((int)&config, 4.0);
    pwm_init(slice, (int)&config, true);
    while (1) {
        wfi();
        if (getchar_timeout_us(500000) == 3)
            break;
    }
    irq_set_enabled(PWM_IRQ_WRAP_0, false);
    pwm_set_irq_enabled(slice, false);
    return 0;
}
