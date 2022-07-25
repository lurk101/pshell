```
    // NOTE: All of these functions can be used in the same fashion as their equivalent
    // libc or Pico SDK functions. Consult those relevant docs.

    // stdio
    close, getchar, getchar_timeout_us, lseek, open, printf, putchar, read, remove,
    rename, screen_height, screen_width, sprintf, write

    // stdlib
    atoi, exit, free, malloc, popcount, rand, srand

    // string
    memcmp, memcpy, memset, strcat, strchr, strcmp, strcpy, strdup, strlen, strncat,
    strncmp, strncpy, strrchr

    // math
    acosf, acoshf, asinf, asinhf, atan2f, atanf, atanhf, cosf, coshf, log10f, logf, powf,
    sinf, sinhf, sqrtf, tanf, tanhf

    // sync
    wfi

    // time
    time_us_32, sleep_ms, sleep_us

    // gpio
    gpio_acknowledge_irq, gpio_add_raw_irq_handler, gpio_add_raw_irq_handler_masked,
    gpio_add_raw_irq_handler_with_order_priority, gpio_add_raw_irq_handler_with_order_priority_masked,
    gpio_clr_mask, gpio_deinit, gpio_disable_pulls, gpio_get, gpio_get_all, gpio_get_dir,
    gpio_get_drive_strength, gpio_get_function, gpio_get_irq_event_mask, gpio_get_out_level,
    gpio_get_slew_rate, gpio_init, gpio_init_mask, gpio_is_dir_out, gpio_is_input_hysteresis_enabled,
    gpio_is_pulled_down, gpio_is_pulled_up, gpio_pull_down, gpio_pull_up, gpio_put, gpio_put_all,
    gpio_put_masked, gpio_remove_raw_irq_handler, gpio_remove_raw_irq_handler_masked, gpio_set_dir,
    gpio_set_dir_all_bits, gpio_set_dir_in_masked, gpio_set_dir_masked, gpio_set_dir_out_masked,
    gpio_set_dormant_irq_enabled, gpio_set_drive_strength, gpio_set_function, gpio_set_inover,
    gpio_set_input_enabled, gpio_set_input_hysteresis_enabled, gpio_set_irq_callback,
    gpio_set_irq_enabled, gpio_set_irq_enabled_with_callback, gpio_set_irqover, gpio_set_mask,
    gpio_set_oeover, gpio_set_outover, gpio_set_pulls, gpio_set_slew_rate, gpio_xor_mask
    // pwm
    pwm_advance_count, pwm_clear_irq, pwm_config_set_clkdiv, pwm_config_set_clkdiv_int,
    pwm_config_set_clkdiv_int_frac, pwm_config_set_clkdiv_mode, pwm_config_set_output_polarity,
    pwm_config_set_phase_correct, pwm_config_set_wrap, pwm_force_irq, pwm_get_counter,
    pwm_get_default_config, pwm_get_dreq, pwm_get_irq_status_mask, pwm_gpio_to_channel,
    pwm_gpio_to_slice_num, pwm_init, pwm_retard_count, pwm_set_both_levels, pwm_set_chan_level,
    pwm_set_clkdiv, pwm_set_clkdiv_int_frac, pwm_set_clkdiv_mode, pwm_set_counter, pwm_set_enabled,
    pwm_set_gpio_level, pwm_set_irq_enabled, pwm_set_irq_mask_enabled, pwm_set_mask_enabled,
    pwm_set_output_polarity, pwm_set_phase_correct, pwm_set_wrap
    // adc
    adc_fifo_drain, adc_fifo_get, adc_fifo_get_blocking, adc_fifo_get_level, adc_fifo_is_empty,
    adc_fifo_setup, adc_get_selected_input, adc_gpio_init, adc_init, adc_irq_set_enabled,
    adc_read, adc_run, adc_select_input, adc_set_clkdiv, adc_set_round_robin, adc_set_temp_sensor_enabled

    // clocks
    clock_configure, clock_configure_gpin, clock_get_hz, clock_gpio_init, clock_set_reported_hz,
    clock_stop, clocks_enable_resus, clocks_init, frequency_count_khz, frequency_count_mhz

    // i2c
    i2c_deinit, i2c_get_dreq, i2c_get_hw, i2c_get_read_available, i2c_get_write_available,
    i2c_hw_index, i2c_init, i2c_read_blocking, i2c_read_raw_blocking, i2c_read_timeout_per_char_us,
    i2c_read_timeout_us, i2c_set_baudrate, i2c_set_slave_mode, i2c_write_blocking,
    i2c_write_raw_blocking, i2c_write_timeout_per_char_us, i2c_write_timeout_us

    // spi
    spi_deinit, spi_get_baudrate, spi_get_const_hw, spi_get_dreq, spi_get_hw, spi_get_index,
    spi_init, spi_is_busy, spi_is_readable, spi_is_writable, spi_read16_blocking, spi_read_blocking,
    spi_set_baudrate, spi_set_format, spi_set_slave, spi_write16_blocking, spi_write16_read16_blocking,
    spi_write_blocking, spi_write_read_blocking

    // irq
    irq_add_shared_handler, irq_clear, irq_get_exclusive_handler, irq_get_priority, irq_get_vtable_handler,
    irq_has_shared_handler, irq_init_priorities, irq_is_enabled, irq_remove_handler, irq_set_enabled,
    irq_set_exclusive_handler, irq_set_mask_enabled, irq_set_pending, irq_set_priority, user_irq_claim,
    user_irq_claim_unused, user_irq_is_claimed, user_irq_unclaim
```
