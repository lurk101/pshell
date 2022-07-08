char clk_name[11][8] = {"gpout0", "gpout1", "gpout2", "gpout3", "ref", "sys",
                        "peri",   "usb",    "adc",    "rtc",    0};

int main() {
    int i;
    for (i = 0; clk_name[i][0]; i++)
        printf("%-10s %d Hz\n", clk_name[i], clock_get_hz(i));
    return (CLK_COUNT);
}
