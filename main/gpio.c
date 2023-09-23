#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "esp_log.h"
#include "esp_event.h"
#include "driver/gpio.h"

#include "defines.h"

void gpio_init(void)
{
	gpio_reset_pin(GPIO_OUT1);
	gpio_set_level(GPIO_OUT1, 0);
	gpio_set_direction(GPIO_OUT1, GPIO_MODE_OUTPUT);

	gpio_reset_pin(GPIO_OUT2);
	gpio_set_level(GPIO_OUT2, 0);
	gpio_set_direction(GPIO_OUT2, GPIO_MODE_OUTPUT);
}
