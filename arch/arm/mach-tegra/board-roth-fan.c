/*
 * arch/arm/mach-tegra/board-roth-fan.c
 *
 * Copyright (c) 2012-2013 NVIDIA CORPORATION, All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/gpio.h>
#include <linux/platform_data/pwm_fan.h>
#include <linux/platform_device.h>

#include "gpio-names.h"
#include "devices.h"
#include "board.h"
#include "board-common.h"
#include "board-roth.h"
#include "tegra-board-id.h"

static struct pwm_fan_platform_data fan_data_yltc_8k = {
	.active_steps = MAX_ACTIVE_STATES,
	.active_rpm = {
		0, 3000, 4000, 5000, 6000, 7000, 8000, 9000, 10000, 11000},
	.active_pwm = {0, 158, 227, 230, 235, 240,
				245, 250, 252, 255},
	.active_rru = {50, 1, 1, 1, 1, 1, 1, 1, 1, 1},
	.active_rrd = {50, 1, 1, 1, 1, 1, 1, 1, 1, 1},
	/*Lookup table to get the actual state cap*/
	.state_cap_lookup = {1, 1, 1, 1, 1, 1, 1, 2, 2, 2},
	.pwm_period = 45334,
	.pwm_id = 0,
	.step_time = 100, /*msecs*/
	.state_cap = 1,
	.active_pwm_max = 256,
	.tach_gpio = -1,
};

static struct pwm_fan_platform_data fan_data_delta_6k = {
	.active_steps = MAX_ACTIVE_STATES,
	.active_rpm = {
		0, 1000, 2000, 3000, 4000, 5000, 6000, 7000, 10000, 11000},
	.active_pwm = {0, 80, 110, 150, 235, 240,
				245, 250, 252, 255},
	.active_rru = {40, 2, 1, 1,
						1, 1, 1, 1, 1, 1},
	.active_rrd = {40, 2, 1, 1, 1,
						1, 1, 1, 1, 1},
	.state_cap_lookup = {2, 2, 2, 2, 2, 2, 2, 3, 3, 3},
	.pwm_period = 45334,
	.pwm_id = 0,
	.step_time = 100, /*msecs*/
	.state_cap = 2,
	.active_pwm_max = 256,
	.tach_gpio = TEGRA_GPIO_PU2,
};

static struct platform_device pwm_fan_therm_cooling_device_yltc_8k = {
	.name = "pwm-fan",
	.id = -1,
	.num_resources = 0,
	.dev = {
		.platform_data = &fan_data_yltc_8k,
	},
};

static struct platform_device pwm_fan_therm_cooling_device_delta_6k = {
	.name = "pwm-fan",
	.id = -1,
	.num_resources = 0,
	.dev = {
		.platform_data = &fan_data_delta_6k,
	},
};

int __init roth_fan_init(void)
{
	int err;
	struct board_info board_info;

	tegra_get_board_info(&board_info);

	err = gpio_request(TEGRA_GPIO_PU3, "pwm-fan");
	if (err < 0) {
		pr_err("FAN:gpio request failed\n");
		return err;
	}
	gpio_free(TEGRA_GPIO_PU3);

	if (board_info.board_id == BOARD_P2560) {
		platform_device_register(
				&pwm_fan_therm_cooling_device_delta_6k);
		pr_info("FAN:registering for P2560\n");
	} else {
		platform_device_register(&pwm_fan_therm_cooling_device_yltc_8k);
		pr_info("FAN:registering for P2454\n");
	}
	return 0;
}
