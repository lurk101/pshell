```
    // NOTE: All of these functions can be used in the same fashion as their equivalent
    // libc or Pico SDK functions. Consult those relevant docs.

    // SDK PRINT
 
    printf sprintf

    // LIBC MEMORY

    malloc free

    // LIBC STRING

    strlen strcpy strcmp strcat strdup memcmp

    // LIBC MATH

    atoi sqrtf sinf cosf tanf logf powf

    // miscellaneous

    rand, srand,

    // IO

    getchar, getchar_timeout_us, putchar, open, close, read, write, lseek, rename, remove

    // SDK TIME

    time_us_32 sleep_us sleep_ms

    // SDK GPIO

    gpio_set_function gpio_get_function gpio_set_pulls gpio_pull_up gpio_is_pulled_up
    gpio_pull_down gpio_is_pulled_down gpio_disable_pulls gpio_set_irqover gpio_set_outover
    gpio_set_inover gpio_set_oeover gpio_set_input_enabled gpio_set_input_hysteresis_enabled
    gpio_is_input_hysteresis_enabled gpio_set_slew_rate gpio_get_slew_rate
    gpio_set_drive_strength gpio_get_drive_strength gpio_set_irq_enabled gpio_set_irq_callback
    gpio_set_irq_enabled_with_callback gpio_set_dormant_irq_enabled gpio_get_irq_event_mask
    gpio_acknowledge_irq gpio_add_raw_irq_handler_with_order_priority_masked
    gpio_add_raw_irq_handler_with_order_priority gpio_add_raw_irq_handler_masked
    gpio_add_raw_irq_handler gpio_remove_raw_irq_handler_masked gpio_remove_raw_irq_handler
    gpio_init gpio_deinit gpio_init_mask gpio_get gpio_get_all gpio_set_mask gpio_clr_mask
    gpio_xor_mask gpio_put_masked gpio_put_all gpio_put gpio_get_out_level gpio_set_dir_out_masked
    gpio_set_dir_in_masked gpio_set_dir_masked gpio_set_dir_all_bits gpio_set_dir gpio_is_dir_out
    gpio_get_dir

    // SDK PWM

    pwm_gpio_to_slice_num pwm_gpio_to_channel pwm_config_set_phase_correct pwm_config_set_clkdiv
    pwm_config_set_clkdiv_int_frac pwm_config_set_clkdiv_int pwm_config_set_clkdiv_mode
    pwm_config_set_output_polarity pwm_config_set_wrap pwm_init pwm_get_default_config
    pwm_set_wrap pwm_set_chan_level pwm_set_both_levels pwm_set_gpio_level pwm_get_counter
    pwm_set_counter pwm_advance_count pwm_retard_count pwm_set_clkdiv_int_frac pwm_set_clkdiv
    pwm_set_output_polarity pwm_set_clkdiv_mode pwm_set_phase_correct pwm_set_enabled
    pwm_set_mask_enabled pwm_set_irq_enabled pwm_set_irq_mask_enabled pwm_clear_irq
    pwm_get_irq_status_mask pwm_force_irq pwm_get_dreq

    // SDK ADC

    adc_init adc_gpio_init adc_select_input adc_get_selected_input adc_set_round_robin
    adc_set_temp_sensor_enabled adc_read adc_run adc_set_clkdiv adc_fifo_setup adc_fifo_is_empty
    adc_fifo_get_level adc_fifo_get adc_fifo_get_blocking adc_fifo_drain adc_irq_set_enabled

    // SDK I2C

    i2c_init i2c_deinit i2c_set_baudrate i2c_set_slave_mode i2c_hw_index i2c_get_hw
    i2c_write_blocking_until i2c_read_blocking_until i2c_write_timeout_us i2c_write_timeout_per_char_us
    i2c_read_timeout_us i2c_read_timeout_per_char_us i2c_write_blocking i2c_read_blocking
    i2c_get_write_available i2c_get_read_available i2c_write_raw_blocking i2c_read_raw_blocking
    i2c_get_dreq

    // SDK SPI

    spi_init spi_deinit spi_set_baudrate spi_get_baudrate spi_get_index spi_get_hw spi_get_const_hw
    spi_set_format spi_set_slave spi_is_writable spi_is_readable spi_is_busy spi_write_read_blocking
    spi_write_blocking spi_read_blocking spi_write16_read16_blocking spi_write16_blocking
    spi_read16_blocking spi_get_dreq

```
