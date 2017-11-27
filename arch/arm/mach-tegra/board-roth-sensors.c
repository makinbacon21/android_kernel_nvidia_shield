/*
 * arch/arm/mach-tegra/board-roth-sensors.c
 *
 * Copyright (c) 2012-2014 NVIDIA CORPORATION, All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * Neither the name of NVIDIA CORPORATION nor the names of its contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/mpu.h>
#include <linux/gpio.h>
#include <linux/therm_est.h>
#include <linux/nct1008.h>
#include <mach/edp.h>
#include <generated/mach-types.h>

#include "gpio-names.h"
#include "board.h"
#include "board-common.h"
#include "board-roth.h"
#include <linux/platform/tegra/cpu-tegra.h>
#include "devices.h"
#include "tegra-board-id.h"

static struct nct1008_platform_data roth_nct1008_pdata = {
	.supported_hwrev = true,
	.extended_range = true,
	.conv_rate = 0x08,
	.loc_name = "soc",

	.sensors = {
		[LOC] = {
			.shutdown_limit = 120, /* C */
			.num_trips = 0,
			.tzp = NULL,
		},
		[EXT] = {
			.shutdown_limit = 91, /* C */
			.num_trips = 2,
			.tzp = NULL,

			.passive_delay = 2000,

			/* Thermal Throttling */
			.trips = {
				{
					.cdev_type = "tegra-balanced",
					.trip_temp = 80000,
					.trip_type = THERMAL_TRIP_PASSIVE,
					.upper = THERMAL_NO_LIMIT,
					.lower = THERMAL_NO_LIMIT,
					.hysteresis = 0,
					.mask = 1,
				},
				{
					.cdev_type = "tegra-hard",
					/*shutdown_limit - 2C*/
					.trip_temp = 86000,
					.trip_type = THERMAL_TRIP_PASSIVE,
					.upper = 1,
					.lower = 1,
					.hysteresis = 6000,
					.mask = 1,
				},
				{
					.cdev_type = "suspend_soctherm",
					.trip_temp = 50000,
					.trip_type = THERMAL_TRIP_ACTIVE,
					.upper = 1,
					.lower = 1,
					.hysteresis = 5000,
					.mask = 1,
				}
			},
		}
	}
};

static struct i2c_board_info roth_i2c4_nct1008_board_info[] = {
	{
		I2C_BOARD_INFO("nct1008", 0x4C),
		.platform_data = &roth_nct1008_pdata,
		.irq = -1,
	}
};

/* MPU board file definition	*/
static struct mpu_platform_data mpu6050_gyro_data = {
	.int_config	= 0x10,
	.level_shifter	= 0,
	/* Located in board_[platformname].h */
	.orientation	= MPU_GYRO_ORIENTATION,
	.sec_slave_type	= SECONDARY_SLAVE_TYPE_NONE,
	.key		= {0x4E, 0xCC, 0x7E, 0xEB, 0xF6, 0x1E, 0x35, 0x22,
			   0x00, 0x34, 0x0D, 0x65, 0x32, 0xE9, 0x94, 0x89},
};

static struct i2c_board_info __initdata inv_mpu6050_i2c2_board_info[] = {
	{
		I2C_BOARD_INFO(MPU_GYRO_NAME, MPU_GYRO_ADDR),
		.platform_data = &mpu6050_gyro_data,
	},
};

static void mpuirq_init(void)
{
	int ret = 0;
	unsigned gyro_irq_gpio = MPU_GYRO_IRQ_GPIO;
	unsigned gyro_bus_num = MPU_GYRO_BUS_NUM;
	char *gyro_name = MPU_GYRO_NAME;

	pr_info("*** MPU START *** mpuirq_init...\n");

	ret = gpio_request(gyro_irq_gpio, gyro_name);

	if (ret < 0) {
		pr_err("%s: gpio_request failed %d\n", __func__, ret);
		return;
	}

	ret = gpio_direction_input(gyro_irq_gpio);
	if (ret < 0) {
		pr_err("%s: gpio_direction_input failed %d\n", __func__, ret);
		gpio_free(gyro_irq_gpio);
		return;
	}
	pr_info("*** MPU END *** mpuirq_init...\n");

	inv_mpu6050_i2c2_board_info[0].irq = gpio_to_irq(MPU_GYRO_IRQ_GPIO);
	i2c_register_board_info(gyro_bus_num, inv_mpu6050_i2c2_board_info,
		ARRAY_SIZE(inv_mpu6050_i2c2_board_info));
}

static int roth_nct1008_init(void)
{
	int nct1008_port = TEGRA_GPIO_PX6;
	int ret = 0;

	tegra_platform_edp_init(roth_nct1008_pdata.sensors[EXT].trips,
				&roth_nct1008_pdata.sensors[EXT].num_trips,
				0); /* edp temperature margin */
	tegra_add_all_vmin_trips(roth_nct1008_pdata.sensors[EXT].trips,
				&roth_nct1008_pdata.sensors[EXT].num_trips);
	tegra_add_cpu_vmax_trips(roth_nct1008_pdata.sensors[EXT].trips,
				&roth_nct1008_pdata.sensors[EXT].num_trips);
	tegra_add_core_edp_trips(roth_nct1008_pdata.sensors[EXT].trips,
				&roth_nct1008_pdata.sensors[EXT].num_trips);

	roth_i2c4_nct1008_board_info[0].irq = gpio_to_irq(nct1008_port);
	pr_info("%s: roth nct1008 irq %d", __func__, \
				roth_i2c4_nct1008_board_info[0].irq);

	ret = gpio_request(nct1008_port, "temp_alert");
	if (ret < 0)
		return ret;

	ret = gpio_direction_input(nct1008_port);
	if (ret < 0) {
		pr_info("%s: calling gpio_free(nct1008_port)", __func__);
		gpio_free(nct1008_port);
	}

	/* roth has thermal sensor on GEN1-I2C i.e. instance 0 */
	i2c_register_board_info(0, roth_i2c4_nct1008_board_info,
		ARRAY_SIZE(roth_i2c4_nct1008_board_info));

	return ret;
}

int __init roth_sensors_init(void)
{
	int err;

	err = roth_nct1008_init();
	if (err)
		return err;

	mpuirq_init();

	return 0;
}
