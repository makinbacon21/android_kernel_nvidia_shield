/*
 * drivers/video/tegra/dc/of_dc.c
 *
 * Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/ktime.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/regulator/consumer.h>
#include <linux/of_platform.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/of_i2c.h>
#include <linux/nvhost.h>
#include <linux/timer.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pinctrl/pinconf-tegra.h>

#include <mach/clk.h>
#include <mach/dc.h>
#include <mach/fb.h>
#include <mach/latency_allowance.h>

#include <tegra/mc.h>

#include "dc_reg.h"
#include "dc_config.h"
#include "dc_priv.h"
#include "dev.h"
#include "nvsd.h"
#include "dsi.h"
#include "edid.h"
#include "hdmi2.0.h"

#ifdef CONFIG_OF
/* #define OF_DC_DEBUG	1 */

#undef OF_DC_LOG
#ifdef OF_DC_DEBUG
#define OF_DC_LOG(fmt, args...) pr_info("OF_DC_LOG: " fmt, ## args)
#else
#define OF_DC_LOG(fmt, args...)
#endif

static struct regulator *of_hdmi_vddio;
static struct regulator *of_hdmi_reg;
static struct regulator *of_hdmi_pll;

#ifdef CONFIG_TEGRA_DC_CMU
static struct tegra_dc_cmu default_cmu = {
	/* lut1 maps sRGB to linear space. */
	{
		0,    1,    2,    4,    5,    6,    7,    9,
		10,   11,   12,   14,   15,   16,   18,   20,
		21,   23,   25,   27,   29,   31,   33,   35,
		37,   40,   42,   45,   48,   50,   53,   56,
		59,   62,   66,   69,   72,   76,   79,   83,
		87,   91,   95,   99,   103,  107,  112,  116,
		121,  126,  131,  136,  141,  146,  151,  156,
		162,  168,  173,  179,  185,  191,  197,  204,
		210,  216,  223,  230,  237,  244,  251,  258,
		265,  273,  280,  288,  296,  304,  312,  320,
		329,  337,  346,  354,  363,  372,  381,  390,
		400,  409,  419,  428,  438,  448,  458,  469,
		479,  490,  500,  511,  522,  533,  544,  555,
		567,  578,  590,  602,  614,  626,  639,  651,
		664,  676,  689,  702,  715,  728,  742,  755,
		769,  783,  797,  811,  825,  840,  854,  869,
		884,  899,  914,  929,  945,  960,  976,  992,
		1008, 1024, 1041, 1057, 1074, 1091, 1108, 1125,
		1142, 1159, 1177, 1195, 1213, 1231, 1249, 1267,
		1286, 1304, 1323, 1342, 1361, 1381, 1400, 1420,
		1440, 1459, 1480, 1500, 1520, 1541, 1562, 1582,
		1603, 1625, 1646, 1668, 1689, 1711, 1733, 1755,
		1778, 1800, 1823, 1846, 1869, 1892, 1916, 1939,
		1963, 1987, 2011, 2035, 2059, 2084, 2109, 2133,
		2159, 2184, 2209, 2235, 2260, 2286, 2312, 2339,
		2365, 2392, 2419, 2446, 2473, 2500, 2527, 2555,
		2583, 2611, 2639, 2668, 2696, 2725, 2754, 2783,
		2812, 2841, 2871, 2901, 2931, 2961, 2991, 3022,
		3052, 3083, 3114, 3146, 3177, 3209, 3240, 3272,
		3304, 3337, 3369, 3402, 3435, 3468, 3501, 3535,
		3568, 3602, 3636, 3670, 3705, 3739, 3774, 3809,
		3844, 3879, 3915, 3950, 3986, 4022, 4059, 4095,
	},
	/* csc */
	{
		0,
	},
	/*lut2*/
	{
		0,
	}
};
#endif
#endif

#ifdef CONFIG_OF

static int out_type_from_pn(struct device_node *panel_node)
{
	struct device_node *default_out_np = NULL;
	u32 temp;

	if (panel_node && of_device_is_available(panel_node))
		default_out_np = of_get_child_by_name(panel_node,
			"disp-default-out");
	if (default_out_np && !of_property_read_u32(default_out_np,
		"nvidia,out-type", &temp)) {
		return (int)temp;
	} else
		return -EINVAL;
}

static int parse_dc_out_type(struct device_node *np,
		struct platform_device *ndev,
		struct tegra_dc_out *default_out)
{
	struct device_node *np_target_disp = NULL;
	int out_type;

	if (ndev->id == 0)
		np_target_disp = tegra_primary_panel_get_dt_node(NULL);
	else
		np_target_disp = tegra_secondary_panel_get_dt_node(NULL);

	out_type = out_type_from_pn(np_target_disp);
	if (out_type >= 0) {
		default_out->type = out_type;
		return 0;
	}
	pr_err("can not determine display type\n");
	return -EINVAL;
}

static int parse_tmds(struct device_node *np,
	u8 *addr)
{
	u32 temp;
	int i = 0;
	u32 major = 0;
	u32 minor = 0;
	struct property *prop;
	const __be32 *p;
	u32 u;
	struct tmds_config *tmds_cfg_addr;
	tmds_cfg_addr = (struct tmds_config *)addr;

	of_property_for_each_u32(np, "version", prop, p, u)
		i++;

	if (i == 2) { /* major version, minor version */
		i = 0;
		of_property_for_each_u32(np,
			"version", prop, p, u) {
			i++;
			if (i == 1)
				major = u;
			else
				minor = u;
		}
		tmds_cfg_addr->version = MKDEV(major, minor);
		OF_DC_LOG("tmds version 0x%x\n",
			tmds_cfg_addr->version);
	} else if (i == 0) {
		OF_DC_LOG("there's no tmds conf version.\n");
	} else {
		OF_DC_LOG("need to have major, minor version\n");
		goto parse_tmds_fail;
	}

	if (!of_property_read_u32(np, "pclk", &temp)) {
		tmds_cfg_addr->pclk = (int)temp;
		OF_DC_LOG("tmds pclk %d\n", temp);
	}
	if (!of_property_read_u32(np, "pll0", &temp)) {
		tmds_cfg_addr->pll0 = (u32)temp;
		OF_DC_LOG("tmds pll0 0x%x\n", temp);
	}
	if (!of_property_read_u32(np, "pll1", &temp)) {
		tmds_cfg_addr->pll1 = (u32)temp;
		OF_DC_LOG("tmds pll1 0x%x\n", temp);
	}
	if (!of_property_read_u32(np, "pe-current", &temp)) {
		tmds_cfg_addr->pe_current = (u32)temp;
		OF_DC_LOG("tmds pe-current 0x%x\n", temp);
	}
	if (!of_property_read_u32(np, "drive-current", &temp)) {
		tmds_cfg_addr->drive_current = (u32)temp;
		OF_DC_LOG("tmds drive-current 0x%x\n", temp);
	}
	if (!of_property_read_u32(np, "peak-current", &temp)) {
		tmds_cfg_addr->peak_current = (u32)temp;
		OF_DC_LOG("tmds peak-current 0x%x\n", temp);
	}
	if (!of_property_read_u32(np, "pad-ctls0-mask", &temp)) {
		tmds_cfg_addr->pad_ctls0_mask = (u32)temp;
		OF_DC_LOG("tmds pad_ctls0_mask 0x%x\n", temp);
	}
	if (!of_property_read_u32(np, "pad-ctls0-setting", &temp)) {
		tmds_cfg_addr->pad_ctls0_setting = (u32)temp;
		OF_DC_LOG("tmds pad_ctls0_setting 0x%x\n", temp);
	}

	return 0;
parse_tmds_fail:
	pr_err("parse tmds fail!\n");
	return -EINVAL;
}

static bool is_dc_default_out_flag(u32 flag)
{
	if ((flag == TEGRA_DC_OUT_HOTPLUG_HIGH) |
		(flag == TEGRA_DC_OUT_HOTPLUG_LOW) |
		(flag == TEGRA_DC_OUT_NVHDCP_POLICY_ALWAYS_ON) |
		(flag == TEGRA_DC_OUT_NVHDCP_POLICY_ON_DEMAND) |
		(flag == TEGRA_DC_OUT_CONTINUOUS_MODE) |
		(flag == TEGRA_DC_OUT_ONE_SHOT_MODE) |
		(flag == TEGRA_DC_OUT_N_SHOT_MODE) |
		(flag == TEGRA_DC_OUT_ONE_SHOT_LP_MODE) |
		(flag == TEGRA_DC_OUT_INITIALIZED_MODE) |
		(flag == TEGRA_DC_OUT_HOTPLUG_WAKE_LP0))
		return true;
	else
		return false;
}

static int parse_disp_default_out(struct platform_device *ndev,
		struct device_node *np,
		struct tegra_dc_out *default_out,
		struct tegra_fb_data *fb)
{
	u32 temp;
	int hotplug_gpio = 0;
	enum of_gpio_flags flags;
	struct device_node *ddc;
	struct device_node *np_hdmi =
		of_find_node_by_path(HDMI_NODE);
	struct device_node *np_sor =
		(ndev->id) ? of_find_node_by_path(SOR1_NODE) :
		of_find_node_by_path(SOR_NODE);
	struct property *prop;
	const __be32 *p;
	u32 u;
	const char *temp_str0;
	u32 n_outpins = 0;
	u8 *addr;
	int err = 0;

	/*
	 * construct default_out
	 */
	if (!of_property_read_u32(np, "nvidia,out-width", &temp)) {
		default_out->width = (unsigned) temp;
		OF_DC_LOG("out_width %d\n", default_out->width);
	}
	if (!of_property_read_u32(np, "nvidia,out-height", &temp)) {
		default_out->height = (unsigned) temp;
		OF_DC_LOG("out_height %d\n", default_out->height);
	}
	if (!of_property_read_u32(np, "nvidia,out-rotation", &temp)) {
		default_out->rotation = (unsigned) temp;
		OF_DC_LOG("out_rotation %d\n", temp);
	}
	if (np_hdmi && of_device_is_available(np_hdmi) &&
		(default_out->type == TEGRA_DC_OUT_HDMI)) {
		int id;
		ddc = of_parse_phandle(np_hdmi, "nvidia,ddc-i2c-bus", 0);

		if (!ddc) {
			pr_err("No ddc device node\n");
			err = -EINVAL;
			goto parse_disp_defout_fail;
		} else
			id = of_alias_get_id(ddc, "i2c");

		of_node_put(ddc);

		if (id >= 0) {
			default_out->dcc_bus = id;
			OF_DC_LOG("out_dcc bus %d\n", id);
		} else {
			pr_err("Invalid i2c id\n");
			err = -EINVAL;
			goto parse_disp_defout_fail;
		}

		hotplug_gpio = of_get_named_gpio_flags(np_hdmi,
				"nvidia,hpd-gpio", 0, &flags);
		default_out->hotplug_gpio = hotplug_gpio;
	}
	if (np_sor && of_device_is_available(np_sor) &&
		((default_out->type == TEGRA_DC_OUT_DP) ||
		(default_out->type == TEGRA_DC_OUT_NVSR_DP))) {
		hotplug_gpio = of_get_named_gpio_flags(np_sor,
				"nvidia,hpd-gpio", 0, &flags);
		if (hotplug_gpio != 0)
			default_out->hotplug_gpio = hotplug_gpio;
	}

	if (!of_property_read_u32(np, "nvidia,out-max-pixclk", &temp)) {
		default_out->max_pixclock = (unsigned)temp;
		OF_DC_LOG("%u max_pixclock in pico second unit\n",
			default_out->max_pixclock);
	}

	if (!of_property_read_u32(np, "nvidia,dither", &temp)) {
		default_out->dither = (unsigned)temp;
		OF_DC_LOG("dither option %d\n",
			default_out->dither);
	}

	of_property_for_each_u32(np, "nvidia,out-flags", prop, p, u) {
		if (!is_dc_default_out_flag(u)) {
			pr_err("invalid out flags\n");
			err = -EINVAL;
			goto parse_disp_defout_fail;
		}
		default_out->flags |= (unsigned) u;
	}
	/* if hotplug not supported clear TEGRA_DC_OUT_HOTPLUG_WAKE_LP0 */
	if (default_out->hotplug_gpio < 0)
		default_out->flags &= ~TEGRA_DC_OUT_HOTPLUG_WAKE_LP0;

	OF_DC_LOG("default_out flag %u\n", default_out->flags);

	if (!of_property_read_u32(np, "nvidia,out-align", &temp)) {
		if (temp == TEGRA_DC_ALIGN_MSB)
			OF_DC_LOG("tegra dc align msb\n");
		else if (temp == TEGRA_DC_ALIGN_LSB)
			OF_DC_LOG("tegra dc align lsb\n");
		else {
			pr_err("invalid out align\n");
			err = -EINVAL;
			goto parse_disp_defout_fail;
		}
		default_out->align = (unsigned)temp;
	}

	if (!of_property_read_u32(np, "nvidia,out-order", &temp)) {
		if (temp == TEGRA_DC_ORDER_RED_BLUE)
			OF_DC_LOG("tegra order red to blue\n");
		else if (temp == TEGRA_DC_ORDER_BLUE_RED)
			OF_DC_LOG("tegra order blue to red\n");
		else {
			pr_err("invalid out order\n");
			err = -EINVAL;
			goto parse_disp_defout_fail;
		}
		default_out->order = (unsigned)temp;
	}

	of_property_for_each_u32(np, "nvidia,out-pins", prop, p, u)
		n_outpins++;

	if ((n_outpins & 0x1) != 0) {
		pr_err("should have name, polarity pair!\n");
		err = -EINVAL;
		goto parse_disp_defout_fail;
	}
	n_outpins = n_outpins/2;
	default_out->n_out_pins = (unsigned)n_outpins;
	if (n_outpins)
		default_out->out_pins = devm_kzalloc(&ndev->dev,
			n_outpins * sizeof(struct tegra_dc_out_pin),
			GFP_KERNEL);

	if (n_outpins && !default_out->out_pins) {
		dev_err(&ndev->dev, "not enough memory\n");
		err = -ENOMEM;
		goto parse_disp_defout_fail;
	}
	n_outpins = 0;
	addr = (u8 *)default_out->out_pins;

	/*
	 * There's no below iteration in case of NULL addr
	 */
	of_property_for_each_u32(np, "nvidia,out-pins", prop, p, u) {
		if ((n_outpins & 0x1) == 0)
			((struct tegra_dc_out_pin *)addr)->name = (int)u;
		else {
			((struct tegra_dc_out_pin *)addr)->pol = (int)u;
			addr += sizeof(struct tegra_dc_out_pin);
		}
		n_outpins++;
	}

	if (!of_property_read_string(np, "nvidia,out-parent-clk",
		&temp_str0)) {
		default_out->parent_clk = temp_str0;
		OF_DC_LOG("parent clk %s\n",
			default_out->parent_clk);
	} else
		pr_info("%s: No parent clk. dft clk will be used.\n",
			__func__);

	if (default_out->type == TEGRA_DC_OUT_HDMI) {
		default_out->depth = 0;
#ifdef CONFIG_FRAMEBUFFER_CONSOLE
		if (!of_property_read_u32(np,
			"nvidia,out-depth", &temp)) {
			default_out->depth = (unsigned) temp;
			OF_DC_LOG("out-depth for HDMI FB console %d\n", temp);
		}
#endif
	} else {
		/* default_out->type == TEGRA_DC_OUT_DSI or
		 * default_out->type == TEGRA_DC_OUT_DP or
		 * default_out->type == TEGRA_DC_OUT_NVSR_DP or
		 * default_out->type == TEGRA_DC_OUT_LVDS
		 */
		if (!of_property_read_u32(np,
			"nvidia,out-depth", &temp)) {
			default_out->depth = (unsigned) temp;
			OF_DC_LOG("out-depth for DSI display %d\n", temp);
		}
	}

	if (!of_property_read_u32(np,
			"nvidia,out-hotplug-state", &temp)) {
			default_out->hotplug_state = (unsigned) temp;
			OF_DC_LOG("out-hotplug-state %d\n", temp);
	}

	/*
	 * construct fb
	 */
	fb->win = 0; /* set fb->win to 0 in default */

	if (!of_property_read_u32(np, "nvidia,out-xres", &temp)) {
		fb->xres = (int)temp;
		OF_DC_LOG("framebuffer xres %d\n", fb->xres);
	}
	if (!of_property_read_u32(np, "nvidia,out-yres", &temp)) {
		fb->yres = (int)temp;
		OF_DC_LOG("framebuffer yres %d\n", fb->yres);
	}
parse_disp_defout_fail:
	of_node_put(np_hdmi);
	of_node_put(np_sor);

	return err;
}

static int parse_tmds_config(struct platform_device *ndev,
	struct device_node *np, struct tegra_dc_out *default_out)
{
	int err = 0;
	u8 *addr;
	struct device_node *tmds_np = NULL;
	struct device_node *entry = NULL;

	tmds_np = of_get_child_by_name(np, "tmds-config");

	if (!tmds_np) {
		pr_info("%s: No tmds-config node\n",
			__func__);
	} else {
		int tmds_set_count =
			of_get_child_count(tmds_np);
		if (!tmds_set_count) {
			pr_info("tmds node exists but no cfg!\n");
			goto success_tmds_config;
		}

		default_out->hdmi_out->n_tmds_config =
			tmds_set_count;

		default_out->hdmi_out->tmds_config = devm_kzalloc(&ndev->dev,
			tmds_set_count * sizeof(struct tmds_config),
			GFP_KERNEL);
		if (!default_out->hdmi_out->tmds_config) {
			dev_err(&ndev->dev, "not enough memory\n");
			return -ENOMEM;
		}
		addr = (u8 *)default_out->hdmi_out->tmds_config;
		for_each_child_of_node(tmds_np, entry) {
			err = parse_tmds(entry, addr);
			if (err)
				goto fail_tmds_config;
			addr += sizeof(struct tmds_config);
		}
	}
success_tmds_config:
	return 0;

fail_tmds_config:
	pr_err("%s: a parse error\n", __func__);
	return -EINVAL;
}

static int parse_sd_settings(struct device_node *np,
	struct tegra_dc_sd_settings *sd_settings)
{
	struct property *prop;
	const __be32 *p;
	u32 u;
	const char *sd_str1;
	u8 coeff[3] = {0, };
	u8 fc[2] = {0, };
	u32 blp[2] = {0, };

	int coeff_count = 0;
	int fc_count = 0;
	int blp_count = 0;
	int bltf_count = 0;
	u8 *addr;
	int sd_lut[108] = {0, };
	int sd_i = 0;
	int  sd_j = 0;
	int sd_index = 0;
	u32 temp;

	if (of_device_is_available(np)) {
		sd_settings->enable = (unsigned) 1;
		sd_settings->enable_int = (unsigned) 1;
	} else {
		sd_settings->enable = (unsigned) 0;
		sd_settings->enable_int = (unsigned) 0;
	}

	OF_DC_LOG("nvidia,sd-enable %d\n", sd_settings->enable);

	if (!of_property_read_u32(np, "nvidia,use-auto-pwm", &temp)) {
		sd_settings->use_auto_pwm = (bool) temp;
		OF_DC_LOG("nvidia,use-auto-pwm %d\n", temp);
	}
	if (!of_property_read_u32(np, "nvidia,hw-update-delay", &temp)) {
		sd_settings->hw_update_delay = (u8) temp;
		OF_DC_LOG("nvidia,hw-update-delay %d\n", temp);
	}
	if (!of_property_read_u32(np, "nvidia,bin-width", &temp)) {
		s32 s32_val;
		s32_val = (s32)temp;
		sd_settings->bin_width = (short)s32_val;
		OF_DC_LOG("nvidia,bin-width %d\n", s32_val);
	}
	if (!of_property_read_u32(np, "nvidia,aggressiveness", &temp)) {
		sd_settings->aggressiveness = (u8) temp;
		OF_DC_LOG("nvidia,aggressiveness %d\n", temp);
	}
	if (!of_property_read_u32(np, "nvidia,use-vid-luma", &temp)) {
		sd_settings->use_vid_luma = (bool) temp;
		OF_DC_LOG("nvidia,use-vid-luma %d\n", temp);
	}
	if (!of_property_read_u32(np, "nvidia,phase-in-settings", &temp)) {
		sd_settings->phase_in_settings = (u8) temp;
		OF_DC_LOG("nvidia,phase-in-settings  %d\n", temp);
	}
	if (!of_property_read_u32(np,
		"nvidia,phase-in-adjustments", &temp)) {
		sd_settings->phase_in_adjustments = (u8) temp;
		OF_DC_LOG("nvidia,phase-in-adjustments  %d\n", temp);
	}
	if (!of_property_read_u32(np, "nvidia,k-limit-enable", &temp)) {
		sd_settings->k_limit_enable = (bool) temp;
		OF_DC_LOG("nvidia,k-limit-enable  %d\n", temp);
	}
	if (!of_property_read_u32(np, "nvidia,k-limit", &temp)) {
		sd_settings->k_limit = (u16) temp;
		OF_DC_LOG("nvidia,k-limit  %d\n", temp);
	}
	if (!of_property_read_u32(np, "nvidia,sd-window-enable", &temp)) {
		sd_settings->sd_window_enable = (bool) temp;
		OF_DC_LOG("nvidia,sd-window-enable  %d\n", temp);
	}
	if (!of_property_read_u32(np,
		"nvidia,soft-clipping-enable", &temp)) {
		sd_settings->soft_clipping_enable = (bool) temp;
		OF_DC_LOG("nvidia,soft-clipping-enable %d\n", temp);
	}
	if (!of_property_read_u32(np,
		"nvidia,soft-clipping-threshold", &temp)) {
		sd_settings->soft_clipping_threshold = (u8) temp;
		OF_DC_LOG("nvidia,soft-clipping-threshold %d\n", temp);
	}
	if (!of_property_read_u32(np, "nvidia,smooth-k-enable", &temp)) {
		sd_settings->smooth_k_enable = (bool) temp;
		OF_DC_LOG("nvidia,smooth-k-enable %d\n", temp);
	}
	if (!of_property_read_u32(np, "nvidia,smooth-k-incr", &temp)) {
		sd_settings->smooth_k_incr = (u16) temp;
		OF_DC_LOG("nvidia,smooth-k-incr %d\n", temp);
	}

	sd_settings->sd_brightness = &sd_brightness;

	if (!of_property_read_u32(np, "nvidia,use-vpulse2", &temp)) {
		sd_settings->use_vpulse2 = (bool) temp;
		OF_DC_LOG("nvidia,use-vpulse2 %d\n", temp);
	}

	if (!of_property_read_string(np, "nvidia,bl-device-name",
		&sd_str1)) {
		sd_settings->bl_device_name = (char *)sd_str1;
		OF_DC_LOG("nvidia,bl-device-name %s\n", sd_str1);
	}

	coeff_count = 0;
	of_property_for_each_u32(np, "nvidia,coeff", prop, p, u)
		coeff_count++;

	if (coeff_count > (sizeof(coeff) / sizeof(coeff[0]))) {
		pr_err("sd_coeff overflow\n");
		return -EINVAL;
	} else {
		coeff_count = 0;
		of_property_for_each_u32(np, "nvidia,coeff", prop, p, u)
			coeff[coeff_count++] = (u8)u;
		sd_settings->coeff.r = coeff[0];
		sd_settings->coeff.g = coeff[1];
		sd_settings->coeff.b = coeff[2];
		OF_DC_LOG("nvidia,coeff %d %d %d\n",
				coeff[0], coeff[1], coeff[2]);
	}
	fc_count = 0;
	of_property_for_each_u32(np, "nvidia,fc", prop, p, u)
		fc_count++;

	if (fc_count > sizeof(fc) / sizeof(fc[0])) {
		pr_err("sd fc overflow\n");
		return -EINVAL;
	} else {
		fc_count = 0;
		of_property_for_each_u32(np, "nvidia,fc", prop, p, u)
		fc[fc_count++] = (u8)u;

		sd_settings->fc.time_limit = fc[0];
		sd_settings->fc.threshold = fc[1];
		OF_DC_LOG("nvidia,fc %d %d\n", fc[0], fc[1]);
	}

	blp_count = 0;
	of_property_for_each_u32(np, "nvidia,blp", prop, p, u)
		blp_count++;

	if (blp_count > sizeof(blp) / sizeof(blp[0])) {
		pr_err("sd blp overflow\n");
		return -EINVAL;
	} else {
		blp_count = 0;
		of_property_for_each_u32(np, "nvidia,blp", prop, p, u)
			blp[blp_count++] = (u32)u;
		sd_settings->blp.time_constant = (u16)blp[0];
		sd_settings->blp.step = (u8)blp[1];
		OF_DC_LOG("nvidia,blp %d %d\n", blp[0], blp[1]);
	}

	bltf_count = 0;
	of_property_for_each_u32(np, "nvidia,bltf", prop, p, u)
		bltf_count++;

	if (bltf_count > (sizeof(sd_settings->bltf) /
			sizeof(sd_settings->bltf[0][0][0]))) {
		pr_err("sd bltf overflow of sd_settings\n");
		return -EINVAL;
	} else {
		addr = &(sd_settings->bltf[0][0][0]);
		of_property_for_each_u32(np, "nvidia,bltf", prop, p, u)
			*(addr++) = u;
	}

	sd_index = 0;
	of_property_for_each_u32(np, "nvidia,lut", prop, p, u)
		sd_index++;

	if (sd_index > sizeof(sd_lut)/sizeof(sd_lut[0])) {
		pr_err("sd lut size overflow of sd_settings\n");
		return -EINVAL;
	} else {
		sd_index = 0;
		of_property_for_each_u32(np, "nvidia,lut", prop, p, u)
			sd_lut[sd_index++] = u;

		sd_index = 0;

		if (prop) {
			for (sd_i = 0; sd_i < 4; sd_i++)
				for (sd_j = 0; sd_j < 9; sd_j++) {
					sd_settings->lut[sd_i][sd_j].r =
						sd_lut[sd_index++];
					sd_settings->lut[sd_i][sd_j].g =
						sd_lut[sd_index++];
					sd_settings->lut[sd_i][sd_j].b =
						sd_lut[sd_index++];
			}
		}
	}
	return 0;
}

static int parse_modes(struct tegra_dc_out *default_out,
						struct device_node *np,
						struct tegra_dc_mode *modes)
{
	u32 temp;
	const struct tegra_dc_out_pin *pins = default_out->out_pins;
	int i;

	if (!of_property_read_u32(np, "clock-frequency", &temp)) {
		modes->pclk = temp;
		OF_DC_LOG("of pclk %d\n", temp);
	} else {
		goto parse_modes_fail;
	}
	if (!of_property_read_u32(np, "nvidia,h-ref-to-sync", &temp)) {
		modes->h_ref_to_sync = temp;
	} else {
		OF_DC_LOG("of h_ref_to_sync %d\n", temp);
		goto parse_modes_fail;
	}
	if (!of_property_read_u32(np, "nvidia,v-ref-to-sync", &temp)) {
		modes->v_ref_to_sync = temp;
	} else {
		OF_DC_LOG("of v_ref_to_sync %d\n", temp);
		goto parse_modes_fail;
	}
	if (!of_property_read_u32(np, "hsync-len", &temp)) {
		modes->h_sync_width = temp;
	} else {
		OF_DC_LOG("of h_sync_width %d\n", temp);
		goto parse_modes_fail;
	}
	if (!of_property_read_u32(np, "vsync-len", &temp)) {
		modes->v_sync_width = temp;
	} else {
		OF_DC_LOG("of v_sync_width %d\n", temp);
		goto parse_modes_fail;
	}
	if (!of_property_read_u32(np, "hback-porch", &temp)) {
		modes->h_back_porch = temp;
	} else {
		OF_DC_LOG("of h_back_porch %d\n", temp);
		goto parse_modes_fail;
	}
	if (!of_property_read_u32(np, "vback-porch", &temp)) {
		modes->v_back_porch = temp;
	} else {
		OF_DC_LOG("of v_back_porch %d\n", temp);
		goto parse_modes_fail;
	}
	if (!of_property_read_u32(np, "hactive", &temp)) {
		modes->h_active = temp;
	} else {
		OF_DC_LOG("of h_active %d\n", temp);
		goto parse_modes_fail;
	}
	if (!of_property_read_u32(np, "vactive", &temp)) {
		modes->v_active = temp;
	} else {
		OF_DC_LOG("of v_active %d\n", temp);
		goto parse_modes_fail;
	}
	if (!of_property_read_u32(np, "hfront-porch", &temp)) {
		modes->h_front_porch = temp;
	} else {
		OF_DC_LOG("of h_front_porch %d\n", temp);
		goto parse_modes_fail;
	}
	if (!of_property_read_u32(np, "vfront-porch", &temp)) {
		modes->v_front_porch = temp;
	} else {
		OF_DC_LOG("of v_front_porch %d\n", temp);
		goto parse_modes_fail;
	}

	for (i = 0; pins && (i < default_out->n_out_pins); i++) {
		switch (pins[i].name) {
		case TEGRA_DC_OUT_PIN_DATA_ENABLE:
			if (pins[i].pol == TEGRA_DC_OUT_PIN_POL_LOW)
				modes->flags |= TEGRA_DC_MODE_FLAG_NEG_DE;
			break;
		case TEGRA_DC_OUT_PIN_H_SYNC:
			if (pins[i].pol == TEGRA_DC_OUT_PIN_POL_LOW)
				modes->flags |= TEGRA_DC_MODE_FLAG_NEG_H_SYNC;
			break;
		case TEGRA_DC_OUT_PIN_V_SYNC:
			if (pins[i].pol == TEGRA_DC_OUT_PIN_POL_LOW)
				modes->flags |= TEGRA_DC_MODE_FLAG_NEG_V_SYNC;
			break;
		default:
			/* Ignore other pin setting */
			break;
		}
	}

	return 0;
parse_modes_fail:
	pr_err("a mode parameter parse fail!\n");
	return -EINVAL;
}

#ifdef CONFIG_TEGRA_DC_CMU
static int parse_cmu_data(struct device_node *np,
	struct tegra_dc_cmu *cmu)
{
	u16 *csc_parse;
	u8 *addr_cmu_lut2;
	struct property *prop;
	const __be32 *p;
	u32 u;
	int csc_count = 0;
	int lut2_count = 0;

	memcpy(cmu, &default_cmu, sizeof(struct tegra_dc_cmu));

	csc_parse = &(cmu->csc.krr);
	addr_cmu_lut2 = &(cmu->lut2[0]);

	of_property_for_each_u32(np, "nvidia,cmu-csc", prop, p, u)
		csc_count++;
	if (csc_count >
		(sizeof(cmu->csc) / sizeof(cmu->csc.krr))) {
		pr_err("cmu csc overflow\n");
		return -EINVAL;
	} else {
		of_property_for_each_u32(np,
			"nvidia,cmu-csc", prop, p, u) {
			OF_DC_LOG("cmu csc 0x%x\n", u);
			*(csc_parse++) = (u16)u;
		}
	}

	of_property_for_each_u32(np, "nvidia,cmu-lut2", prop, p, u)
		lut2_count++;

	if (lut2_count >
		(sizeof(cmu->lut2) / sizeof(cmu->lut2[0]))) {
		pr_err("cmu lut2 overflow\n");
		return -EINVAL;
	} else {
		of_property_for_each_u32(np, "nvidia,cmu-lut2",
			prop, p, u) {
			/* OF_DC_LOG("cmu lut2 0x%x\n", u); */
			*(addr_cmu_lut2++) = (u8)u;
		}
	}
	return 0;
}
#endif

static struct tegra_dsi_cmd *tegra_dsi_parse_cmd_dt(struct platform_device *ndev,
					const struct device_node *node,
					struct property *prop,
					u32 n_cmd)
{
	struct tegra_dsi_cmd *dsi_cmd = NULL, *temp;
	u32 *prop_val_ptr;
	u32 cnt = 0, i = 0;
	u8 arg1 = 0, arg2 = 0, arg3 = 0;
	bool long_pkt = false;

	if (!n_cmd)
		return NULL;

	if (!prop)
		return NULL;
	prop_val_ptr = prop->value;

	dsi_cmd = devm_kzalloc(&ndev->dev, sizeof(*dsi_cmd) * n_cmd,
				GFP_KERNEL);
	if (!dsi_cmd) {
		pr_err("dsi: cmd memory allocation failed\n");
		return ERR_PTR(-ENOMEM);
	}
	temp = dsi_cmd;

	for (cnt  = 0; cnt < n_cmd; cnt++, temp++) {
		temp->cmd_type = be32_to_cpu(*prop_val_ptr++);
		if ((temp->cmd_type == TEGRA_DSI_PACKET_CMD) ||
			(temp->cmd_type ==
			TEGRA_DSI_PACKET_VIDEO_VBLANK_CMD)) {
			temp->data_id = be32_to_cpu(*prop_val_ptr++);
			arg1 = be32_to_cpu(*prop_val_ptr++);
			arg2 = be32_to_cpu(*prop_val_ptr++);
			prop_val_ptr++; /* skip ecc */
			long_pkt = (temp->data_id == DSI_GENERIC_LONG_WRITE ||
				temp->data_id == DSI_DCS_LONG_WRITE ||
				temp->data_id == DSI_NULL_PKT_NO_DATA ||
				temp->data_id == DSI_BLANKING_PKT_NO_DATA) ?
				true : false;
			if (!long_pkt && (temp->cmd_type ==
				TEGRA_DSI_PACKET_VIDEO_VBLANK_CMD))
				arg3 = be32_to_cpu(*prop_val_ptr++);
			if (long_pkt) {
				temp->sp_len_dly.data_len =
					(arg2 << NUMOF_BIT_PER_BYTE) | arg1;
				temp->pdata = devm_kzalloc(&ndev->dev,
					temp->sp_len_dly.data_len, GFP_KERNEL);
				for (i = 0; i < temp->sp_len_dly.data_len; i++)
					(temp->pdata)[i] =
					be32_to_cpu(*prop_val_ptr++);
				prop_val_ptr += 2; /* skip checksum */
			} else {
				temp->sp_len_dly.sp.data0 = arg1;
				temp->sp_len_dly.sp.data1 = arg2;
				if (temp->cmd_type ==
					TEGRA_DSI_PACKET_VIDEO_VBLANK_CMD)
					temp->club_cmd = (bool)arg3;
			}
		} else if (temp->cmd_type == TEGRA_DSI_DELAY_MS) {
			temp->sp_len_dly.delay_ms =
				be32_to_cpu(*prop_val_ptr++);
		} else if (temp->cmd_type == TEGRA_DSI_SEND_FRAME) {
			temp->sp_len_dly.frame_cnt =
				be32_to_cpu(*prop_val_ptr++);
		} else if (temp->cmd_type == TEGRA_DSI_GPIO_SET) {
			temp->sp_len_dly.gpio =
				be32_to_cpu(*prop_val_ptr++);
			temp->data_id =
				be32_to_cpu(*prop_val_ptr++);
		}
	}

	return dsi_cmd;
}

static const u32 *tegra_dsi_parse_pkt_seq_dt(struct platform_device *ndev,
						struct device_node *node,
						struct property *prop)
{
	u32 *prop_val_ptr;
	u32 *pkt_seq;
	int line, i;

#define LINE_STOP 0xff

	if (!prop)
		return NULL;

	pkt_seq = devm_kzalloc(&ndev->dev,
				sizeof(u32) * NUMOF_PKT_SEQ, GFP_KERNEL);
	if (!pkt_seq) {
		dev_err(&ndev->dev,
			"dsi: pkt seq memory allocation failed\n");
		return ERR_PTR(-ENOMEM);
	}
	prop_val_ptr = prop->value;
	for (line = 0; line < NUMOF_PKT_SEQ; line += 2) {
		/* compute line value from dt line */
		for (i = 0;; i += 2) {
			u32 cmd = be32_to_cpu(*prop_val_ptr++);
			if (cmd == LINE_STOP)
				break;
			else if (cmd == PKT_LP)
				pkt_seq[line] |= PKT_LP;
			else {
				u32 len = be32_to_cpu(*prop_val_ptr++);
				if (i == 0) /* PKT_ID0 */
					pkt_seq[line] |=
						PKT_ID0(cmd) | PKT_LEN0(len);
				if (i == 2) /* PKT_ID1 */
					pkt_seq[line] |=
						PKT_ID1(cmd) | PKT_LEN1(len);
				if (i == 4) /* PKT_ID2 */
					pkt_seq[line] |=
						PKT_ID2(cmd) | PKT_LEN2(len);
				if (i == 6) /* PKT_ID3 */
					pkt_seq[line + 1] |=
						PKT_ID3(cmd) | PKT_LEN3(len);
				if (i == 8) /* PKT_ID4 */
					pkt_seq[line + 1] |=
						PKT_ID4(cmd) | PKT_LEN4(len);
				if (i == 10) /* PKT_ID5 */
					pkt_seq[line + 1] |=
						PKT_ID5(cmd) | PKT_LEN5(len);
			}
		}
	}

#undef LINE_STOP

	return pkt_seq;
}

static struct device_node *parse_dsi_settings(struct platform_device *ndev,
	struct device_node *np_dsi,
	struct tegra_dc_platform_data *pdata)
{
	u32 temp;
	int dsi_te_gpio = 0;
	int bl_name_len = 0;
	struct tegra_dsi_out *dsi = pdata->default_out->dsi;
	struct device_node *np_dsi_panel = NULL;
	struct property *prop;
	const __be32 *p;
	u32 u;

	if (ndev->id == 0)
		np_dsi_panel = tegra_primary_panel_get_dt_node(pdata);
	else
		np_dsi_panel = tegra_secondary_panel_get_dt_node(pdata);

	if (!np_dsi_panel) {
		pr_err("There is no valid panel node\n");
		return NULL;
	}

	if (!of_property_read_u32(np_dsi, "nvidia,dsi-controller-vs", &temp)) {
		dsi->controller_vs = (u8)temp;
		if (temp == DSI_VS_0)
			OF_DC_LOG("dsi controller vs DSI_VS_0\n");
		else if (temp == DSI_VS_1)
			OF_DC_LOG("dsi controller vs DSI_VS_1\n");
		else {
			pr_err("invalid dsi controller version\n");
			return NULL;
		}
	}

	if (!of_property_read_u32(np_dsi_panel,
			"nvidia,dsi-n-data-lanes", &temp)) {
		dsi->n_data_lanes = (u8)temp;
		OF_DC_LOG("n data lanes %d\n", dsi->n_data_lanes);
	}
	if (!of_property_read_u32(np_dsi_panel,
			"nvidia,dsi-video-burst-mode", &temp)) {
		dsi->video_burst_mode = (u8)temp;
		if (temp == TEGRA_DSI_VIDEO_NONE_BURST_MODE)
			OF_DC_LOG("dsi video NON_BURST_MODE\n");
		else if (temp == TEGRA_DSI_VIDEO_NONE_BURST_MODE_WITH_SYNC_END)
			OF_DC_LOG("dsi video NONE_BURST_MODE_WITH_SYNC_END\n");
		else if (temp == TEGRA_DSI_VIDEO_BURST_MODE_LOWEST_SPEED)
			OF_DC_LOG("dsi video BURST_MODE_LOWEST_SPEED\n");
		else if (temp == TEGRA_DSI_VIDEO_BURST_MODE_LOW_SPEED)
			OF_DC_LOG("dsi video BURST_MODE_LOW_SPEED\n");
		else if (temp == TEGRA_DSI_VIDEO_BURST_MODE_MEDIUM_SPEED)
			OF_DC_LOG("dsi video BURST_MODE_MEDIUM_SPEED\n");
		else if (temp == TEGRA_DSI_VIDEO_BURST_MODE_FAST_SPEED)
			OF_DC_LOG("dsi video BURST_MODE_FAST_SPEED\n");
		else if (temp == TEGRA_DSI_VIDEO_BURST_MODE_FASTEST_SPEED)
			OF_DC_LOG("dsi video BURST_MODE_FASTEST_SPEED\n");
		else {
			pr_err("invalid dsi video burst mode\n");
			return NULL;
		}
	}
	if (!of_property_read_u32(np_dsi_panel,
			"nvidia,dsi-pixel-format", &temp)) {
		dsi->pixel_format = (u8)temp;
		if (temp == TEGRA_DSI_PIXEL_FORMAT_16BIT_P)
			OF_DC_LOG("dsi pixel format 16BIT_P\n");
		else if (temp == TEGRA_DSI_PIXEL_FORMAT_18BIT_P)
			OF_DC_LOG("dsi pixel format 18BIT_P\n");
		else if (temp == TEGRA_DSI_PIXEL_FORMAT_18BIT_NP)
			OF_DC_LOG("dsi pixel format 18BIT_NP\n");
		else if (temp == TEGRA_DSI_PIXEL_FORMAT_24BIT_P)
			OF_DC_LOG("dsi pixel format 24BIT_P\n");
		else {
			pr_err("invalid dsi pixel format\n");
			return NULL;
		}
	}
	if (!of_property_read_u32(np_dsi_panel,
			"nvidia,dsi-refresh-rate", &temp)) {
		dsi->refresh_rate = (u8)temp;
		OF_DC_LOG("dsi refresh rate %d\n", dsi->refresh_rate);
	}
	if (!of_property_read_u32(np_dsi_panel,
			"nvidia,dsi-rated-refresh-rate", &temp)) {
		dsi->rated_refresh_rate = (u8)temp;
		OF_DC_LOG("dsi rated refresh rate %d\n",
				dsi->rated_refresh_rate);
	}
	if (!of_property_read_u32(np_dsi_panel,
			"nvidia,dsi-virtual-channel", &temp)) {
		dsi->virtual_channel = (u8)temp;
		if (temp == TEGRA_DSI_VIRTUAL_CHANNEL_0)
			OF_DC_LOG("dsi virtual channel 0\n");
		else if (temp == TEGRA_DSI_VIRTUAL_CHANNEL_1)
			OF_DC_LOG("dsi virtual channel 1\n");
		else if (temp == TEGRA_DSI_VIRTUAL_CHANNEL_2)
			OF_DC_LOG("dsi virtual channel 2\n");
		else if (temp == TEGRA_DSI_VIRTUAL_CHANNEL_3)
			OF_DC_LOG("dsi virtual channel 3\n");
		else {
			pr_err("invalid dsi virtual ch\n");
			return NULL;
		}
	}
	if (!of_property_read_u32(np_dsi_panel, "nvidia,dsi-instance", &temp)) {
		dsi->dsi_instance = (u8)temp;
		if (temp == DSI_INSTANCE_0)
			OF_DC_LOG("dsi instance 0\n");
		else if (temp == DSI_INSTANCE_1)
			OF_DC_LOG("dsi instance 1\n");
		else {
			pr_err("invalid dsi instance\n");
			return NULL;
		}
	}
	if (!of_property_read_u32(np_dsi_panel,
		"nvidia,dsi-panel-reset", &temp)) {
		dsi->panel_reset = (u8)temp;
		OF_DC_LOG("dsi panel reset %d\n", dsi->panel_reset);
	}
	if (!of_property_read_u32(np_dsi_panel,
				"nvidia,dsi-te-polarity-low", &temp)) {
		dsi->te_polarity_low = (u8)temp;
		OF_DC_LOG("dsi panel te polarity low %d\n",
			dsi->te_polarity_low);
	}
	if (!of_property_read_u32(np_dsi_panel,
				"nvidia,dsi-lp00-pre-panel-wakeup", &temp)) {
		dsi->lp00_pre_panel_wakeup = (u8)temp;
		OF_DC_LOG("dsi panel lp00 pre panel wakeup %d\n",
				dsi->lp00_pre_panel_wakeup);
	}
	if (of_find_property(np_dsi_panel,
		"nvidia,dsi-bl-name", &bl_name_len)) {
		dsi->bl_name = devm_kzalloc(&ndev->dev,
				sizeof(u8) * bl_name_len, GFP_KERNEL);
		if (!of_property_read_string(np_dsi_panel,
				"nvidia,dsi-bl-name",
				(const char **)&dsi->bl_name))
			OF_DC_LOG("dsi panel bl name %s\n", dsi->bl_name);
		else {
			pr_err("dsi error parsing bl name\n");
			devm_kfree(&ndev->dev, dsi->bl_name);
		}
	}

	if (!of_property_read_u32(np_dsi_panel,
		"nvidia,dsi-ganged-type", &temp)) {
		dsi->ganged_type = (u8)temp;
		OF_DC_LOG("dsi ganged_type %d\n", dsi->ganged_type);
	}

	if (!of_property_read_u32(np_dsi_panel,
		"nvidia,dsi-suspend-aggr", &temp)) {
		dsi->suspend_aggr = (u8)temp;
		OF_DC_LOG("dsi suspend_aggr %d\n", dsi->suspend_aggr);
	}

	if (!of_property_read_u32(np_dsi_panel,
		"nvidia,dsi-edp-bridge", &temp)) {
		dsi->dsi2edp_bridge_enable = (bool)temp;
		OF_DC_LOG("dsi2edp_bridge_enabled %d\n",
			dsi->dsi2edp_bridge_enable);
	}

	if (!of_property_read_u32(np_dsi_panel,
		"nvidia,dsi-lvds-bridge", &temp)) {
		dsi->dsi2lvds_bridge_enable = (bool)temp;
		OF_DC_LOG("dsi-lvds_bridge_enabled %d\n",
			dsi->dsi2lvds_bridge_enable);
	}

	dsi_te_gpio = of_get_named_gpio(np_dsi_panel, "nvidia,dsi-te-gpio", 0);
	if (gpio_is_valid(dsi_te_gpio)) {
		dsi->te_gpio = dsi_te_gpio;
		OF_DC_LOG("dsi te_gpio %d\n", dsi_te_gpio);
	}

	of_property_for_each_u32(np_dsi_panel, "nvidia,dsi-dpd-pads",
		prop, p, u) {
		dsi->dpd_dsi_pads |= (u32)u;
		OF_DC_LOG("dpd_dsi_pads %u\n", dsi->dpd_dsi_pads);
	}

	if (!of_property_read_u32(np_dsi_panel,
		"nvidia,dsi-power-saving-suspend", &temp)) {
		dsi->power_saving_suspend = (bool)temp;
		OF_DC_LOG("dsi power saving suspend %d\n",
			dsi->power_saving_suspend);
	}
	if (!of_property_read_u32(np_dsi_panel,
		"nvidia,dsi-ulpm-not-support", &temp)) {
		dsi->ulpm_not_supported = (bool)temp;
		OF_DC_LOG("dsi ulpm_not_supported %d\n",
			dsi->ulpm_not_supported);
	}
	if (!of_property_read_u32(np_dsi_panel,
		"nvidia,dsi-video-data-type", &temp)) {
		dsi->video_data_type = (u8)temp;
		if (temp == TEGRA_DSI_VIDEO_TYPE_VIDEO_MODE)
			OF_DC_LOG("dsi video type VIDEO_MODE\n");
		else if (temp == TEGRA_DSI_VIDEO_TYPE_COMMAND_MODE)
			OF_DC_LOG("dsi video type COMMAND_MODE\n");
		else {
			pr_err("invalid dsi video data type\n");
			return NULL;
		}
	}
	if (!of_property_read_u32(np_dsi_panel,
		"nvidia,dsi-video-clock-mode", &temp)) {
		dsi->video_clock_mode = (u8)temp;
		if (temp == TEGRA_DSI_VIDEO_CLOCK_CONTINUOUS)
			OF_DC_LOG("dsi video clock mode CONTINUOUS\n");
		else if (temp == TEGRA_DSI_VIDEO_CLOCK_TX_ONLY)
			OF_DC_LOG("dsi video clock mode TX_ONLY\n");
		else {
			pr_err("invalid dsi video clk mode\n");
			return NULL;
		}
	}
	if (!of_property_read_u32(np_dsi_panel,
		"nvidia,dsi-n-init-cmd", &temp)) {
		dsi->n_init_cmd = (u16)temp;
		OF_DC_LOG("dsi n_init_cmd %d\n",
			dsi->n_init_cmd);
	}
	dsi->dsi_init_cmd =
		tegra_dsi_parse_cmd_dt(ndev, np_dsi_panel,
			of_find_property(np_dsi_panel,
			"nvidia,dsi-init-cmd", NULL),
			dsi->n_init_cmd);
	if (dsi->n_init_cmd &&
		IS_ERR_OR_NULL(dsi->dsi_init_cmd)) {
		dev_err(&ndev->dev,
			"dsi: copy init cmd from dt failed\n");
		return NULL;
	};

	if (!of_property_read_u32(np_dsi_panel,
			"nvidia,dsi-n-suspend-cmd", &temp)) {
		dsi->n_suspend_cmd = (u16)temp;
		OF_DC_LOG("dsi n_suspend_cmd %d\n",
			dsi->n_suspend_cmd);
	}
	dsi->dsi_suspend_cmd =
		tegra_dsi_parse_cmd_dt(ndev, np_dsi_panel,
			of_find_property(np_dsi_panel,
			"nvidia,dsi-suspend-cmd", NULL),
			dsi->n_suspend_cmd);
	if (dsi->n_suspend_cmd &&
		IS_ERR_OR_NULL(dsi->dsi_suspend_cmd)) {
		dev_err(&ndev->dev,
			"dsi: copy suspend cmd from dt failed\n");
		return NULL;
	};

	if (!of_property_read_u32(np_dsi_panel,
		"nvidia,dsi-n-early-suspend-cmd", &temp)) {
		dsi->n_early_suspend_cmd = (u16)temp;
		OF_DC_LOG("dsi n_early_suspend_cmd %d\n",
			dsi->n_early_suspend_cmd);
	}
	dsi->dsi_early_suspend_cmd =
		tegra_dsi_parse_cmd_dt(ndev, np_dsi_panel,
			of_find_property(np_dsi_panel,
			"nvidia,dsi-early-suspend-cmd", NULL),
			dsi->n_early_suspend_cmd);
	if (dsi->n_early_suspend_cmd &&
		IS_ERR_OR_NULL(dsi->dsi_early_suspend_cmd)) {
		dev_err(&ndev->dev,
			"dsi: copy early suspend cmd from dt failed\n");
		return NULL;
	};

	if (!of_property_read_u32(np_dsi_panel,
		"nvidia,dsi-n-late-resume-cmd", &temp)) {
		dsi->n_late_resume_cmd = (u16)temp;
		OF_DC_LOG("dsi n_late_resume_cmd %d\n",
			dsi->n_late_resume_cmd);
	}
	dsi->dsi_late_resume_cmd =
		tegra_dsi_parse_cmd_dt(ndev, np_dsi_panel,
			of_find_property(np_dsi_panel,
			"nvidia,dsi-late-resume-cmd", NULL),
			dsi->n_late_resume_cmd);
	if (dsi->n_late_resume_cmd &&
		IS_ERR_OR_NULL(dsi->dsi_late_resume_cmd)) {
		dev_err(&ndev->dev,
			"dsi: copy late resume cmd from dt failed\n");
		return NULL;
	};

	dsi->pkt_seq =
		tegra_dsi_parse_pkt_seq_dt(ndev, np_dsi_panel,
			of_find_property(np_dsi_panel,
			"nvidia,dsi-pkt-seq", NULL));
	if (IS_ERR(dsi->pkt_seq)) {
		dev_err(&ndev->dev,
			"dsi pkt seq from dt fail\n");
		return NULL;
	}

	if (!of_property_read_u32(np_dsi_panel,
		"nvidia,dsi-phy-hsdexit", &temp)) {
		dsi->phy_timing.t_hsdexit_ns = (u16)temp;
		OF_DC_LOG("phy t_hsdexit_ns %d\n",
			dsi->phy_timing.t_hsdexit_ns);
	}

	if (!of_property_read_u32(np_dsi_panel,
		"nvidia,dsi-phy-hstrail", &temp)) {
		dsi->phy_timing.t_hstrail_ns = (u16)temp;
		OF_DC_LOG("phy t_hstrail_ns %d\n",
			dsi->phy_timing.t_hstrail_ns);
	}

	if (!of_property_read_u32(np_dsi_panel,
		"nvidia,dsi-phy-datzero", &temp)) {
		dsi->phy_timing.t_datzero_ns = (u16)temp;
		OF_DC_LOG("phy t_datzero_ns %d\n",
			dsi->phy_timing.t_datzero_ns);
	}

	if (!of_property_read_u32(np_dsi_panel,
			"nvidia,dsi-phy-hsprepare", &temp)) {
		dsi->phy_timing.t_hsprepare_ns = (u16)temp;
		OF_DC_LOG("phy t_hsprepare_ns %d\n",
			dsi->phy_timing.t_hsprepare_ns);
	}

	if (!of_property_read_u32(np_dsi_panel,
			"nvidia,dsi-phy-clktrail", &temp)) {
		dsi->phy_timing.t_clktrail_ns = (u16)temp;
		OF_DC_LOG("phy t_clktrail_ns %d\n",
			dsi->phy_timing.t_clktrail_ns);
	}

	if (!of_property_read_u32(np_dsi_panel,
		"nvidia,dsi-phy-clkpost", &temp)) {
		dsi->phy_timing.t_clkpost_ns = (u16)temp;
		OF_DC_LOG("phy t_clkpost_ns %d\n",
			dsi->phy_timing.t_clkpost_ns);
	}

	if (!of_property_read_u32(np_dsi_panel,
		"nvidia,dsi-phy-clkzero", &temp)) {
		dsi->phy_timing.t_clkzero_ns = (u16)temp;
		OF_DC_LOG("phy t_clkzero_ns %d\n",
			dsi->phy_timing.t_clkzero_ns);
	}

	if (!of_property_read_u32(np_dsi_panel,
		"nvidia,dsi-phy-tlpx", &temp)) {
		dsi->phy_timing.t_tlpx_ns = (u16)temp;
		OF_DC_LOG("phy t_tlpx_ns %d\n",
			dsi->phy_timing.t_tlpx_ns);
	}

	if (!of_property_read_u32(np_dsi_panel,
			"nvidia,dsi-phy-clkprepare", &temp)) {
		dsi->phy_timing.t_clkprepare_ns = (u16)temp;
		OF_DC_LOG("phy t_clkprepare_ns %d\n",
			dsi->phy_timing.t_clkprepare_ns);
	}

	if (!of_property_read_u32(np_dsi_panel,
		"nvidia,dsi-phy-clkpre", &temp)) {
		dsi->phy_timing.t_clkpre_ns = (u16)temp;
		OF_DC_LOG("phy t_clkpre_ns %d\n",
			dsi->phy_timing.t_clkpre_ns);
	}

	if (!of_property_read_u32(np_dsi_panel,
		"nvidia,dsi-phy-wakeup", &temp)) {
		dsi->phy_timing.t_wakeup_ns = (u16)temp;
		OF_DC_LOG("phy t_wakeup_ns %d\n",
			dsi->phy_timing.t_wakeup_ns);
	}

	if (!of_property_read_u32(np_dsi_panel,
		"nvidia,dsi-phy-taget", &temp)) {
		dsi->phy_timing.t_taget_ns = (u16)temp;
		OF_DC_LOG("phy t_taget_ns %d\n",
			dsi->phy_timing.t_taget_ns);
	}

	if (!of_property_read_u32(np_dsi_panel,
		"nvidia,dsi-phy-tasure", &temp)) {
		dsi->phy_timing.t_tasure_ns = (u16)temp;
		OF_DC_LOG("phy t_tasure_ns %d\n",
			dsi->phy_timing.t_tasure_ns);
	}

	if (!of_property_read_u32(np_dsi_panel,
		"nvidia,dsi-phy-tago", &temp)) {
		dsi->phy_timing.t_tago_ns = (u16)temp;
		OF_DC_LOG("phy t_tago_ns %d\n",
			dsi->phy_timing.t_tago_ns);
	}

	if (!of_find_property(np_dsi_panel,
		"nvidia,dsi-boardinfo", NULL)) {
		of_property_read_u32_index(np_dsi_panel,
			"nvidia,dsi-boardinfo", 0,
			&dsi->boardinfo.platform_boardid);
		of_property_read_u32_index(np_dsi_panel,
			"nvidia,dsi-boardinfo", 1,
			&dsi->boardinfo.platform_boardversion);
		of_property_read_u32_index(np_dsi_panel,
			"nvidia,dsi-boardinfo", 2,
			&dsi->boardinfo.display_boardid);
		of_property_read_u32_index(np_dsi_panel,
			"nvidia,dsi-boardinfo", 3,
			&dsi->boardinfo.display_boardversion);

		OF_DC_LOG("boardinfo platform_boardid = %d \
					 platform_boardversion = %d \
					 display_boardid = %d \
					 display_boardversion = %d\n",
					 dsi->boardinfo.platform_boardid,
					 dsi->boardinfo.platform_boardversion,
					 dsi->boardinfo.display_boardid,
					 dsi->boardinfo.display_boardversion);
	}

	return np_dsi_panel;
}

static int parse_lt_setting(struct device_node *np,
	u8 *addr)
{
	int i = 0;
	struct property *prop;
	const __be32 *p;
	u32 u;
	struct tegra_dc_dp_lt_settings *lt_setting_addr;
	u32 temp;
	int n_drive_current =
		sizeof(lt_setting_addr->drive_current)/
		sizeof(lt_setting_addr->drive_current[0]);
	int n_lane_preemphasis =
		sizeof(lt_setting_addr->lane_preemphasis)/
		sizeof(lt_setting_addr->lane_preemphasis[0]);
	int n_post_cursor =
		sizeof(lt_setting_addr->post_cursor)/
		sizeof(lt_setting_addr->post_cursor[0]);


	lt_setting_addr = (struct tegra_dc_dp_lt_settings *)addr;

	of_property_for_each_u32(np, "nvidia,drive-current", prop, p, u) {
		lt_setting_addr->drive_current[i] = (u32)u;
		i++;
	}
	if (n_drive_current != i)
		return -EINVAL;
	i = 0;
	of_property_for_each_u32(np, "nvidia,lane-preemphasis", prop, p, u) {
		lt_setting_addr->lane_preemphasis[i] = (u32)u;
		i++;
	}
	if (n_lane_preemphasis != i)
		return -EINVAL;
	i = 0;
	of_property_for_each_u32(np, "nvidia,post-cursor", prop, p, u) {
		lt_setting_addr->post_cursor[i] = (u32)u;
		i++;
	}
	if (n_post_cursor != i)
		return -EINVAL;

	if (!of_property_read_u32(np, "nvidia,tx-pu", &temp)) {
		lt_setting_addr->tx_pu = (u32)temp;
		OF_DC_LOG("tx_pu %d\n", temp);
	}
	if (!of_property_read_u32(np, "nvidia,load-adj", &temp)) {
		lt_setting_addr->load_adj = (u32)temp;
		OF_DC_LOG("load_adj %d\n", temp);
	}
	return 0;
}

static struct device_node *parse_dp_settings(struct platform_device *ndev,
	struct tegra_dc_platform_data *pdata)
{
	u32 temp;
	u8 *addr;
	struct tegra_dp_out *dpout = pdata->default_out->dp_out;
	struct device_node *np_dp_panel = NULL;
	struct device_node *np_dp_lt_set = NULL;
	struct device_node *entry = NULL;
	int err;

	if (ndev->id == 0)
		np_dp_panel = tegra_primary_panel_get_dt_node(pdata);
	else
		np_dp_panel = tegra_secondary_panel_get_dt_node(pdata);

	if (!np_dp_panel) {
		pr_err("There is no valid panel node\n");
		return NULL;
	}

	np_dp_lt_set =
		of_get_child_by_name(np_dp_panel,
		"dp-lt-settings");

	if (!np_dp_lt_set) {
		pr_info("%s: No dp-lt-settings node\n",
			__func__);
	} else {
		int n_lt_settings =
			of_get_child_count(np_dp_lt_set);
		if (!n_lt_settings) {
			pr_info("lt-settings node has no child node\n");
		} else {
			dpout->n_lt_settings = n_lt_settings;
			dpout->lt_settings = devm_kzalloc(&ndev->dev,
				n_lt_settings *
				sizeof(struct tegra_dc_dp_lt_settings),
				GFP_KERNEL);
			if (!dpout->lt_settings) {
				pr_err("not enough memory\n");
				return NULL;
			}
			addr = (u8 *)dpout->lt_settings;
			for_each_child_of_node(np_dp_lt_set, entry) {
				err = parse_lt_setting(entry, addr);
				if (err)
					return NULL;
				addr += sizeof(
					struct tegra_dc_dp_lt_settings);
			}
		}
	}

	if (!of_property_read_u32(np_dp_panel,
			"nvidia,tx-pu-disable", &temp)) {
		dpout->tx_pu_disable = (bool)temp;
		OF_DC_LOG("tx_pu_disable %d\n", dpout->tx_pu_disable);
	}
	if (!of_property_read_u32(np_dp_panel,
			"nvidia,link-bw", &temp)) {
		dpout->link_bw = (u8)temp;
		OF_DC_LOG("link_bw %d\n", dpout->link_bw);
	}

	return np_dp_panel;
}

static struct device_node *parse_lvds_settings(struct platform_device *ndev,
	struct tegra_dc_platform_data *pdata)
{
	struct device_node *np_lvds_panel = NULL;

	if (ndev->id == 0)
		np_lvds_panel = tegra_primary_panel_get_dt_node(pdata);
	else
		np_lvds_panel = tegra_secondary_panel_get_dt_node(pdata);

	return np_lvds_panel;
}

static int dc_hdmi_out_enable(struct device *dev)
{
	int err = 0;

	struct device_node *np_hdmi =
		of_find_node_by_path(HDMI_NODE);

	if (!np_hdmi || !of_device_is_available(np_hdmi)) {
		pr_info("%s: no valid hdmi node\n", __func__);
		goto dc_hdmi_out_en_fail;
	}

	if (!of_hdmi_reg) {
		of_hdmi_reg = regulator_get(dev, "avdd_hdmi");
		if (IS_ERR_OR_NULL(of_hdmi_reg)) {
			pr_err("hdmi: couldn't get regulator avdd_hdmi\n");
			of_hdmi_reg = NULL;
			err = PTR_ERR(of_hdmi_reg);
			goto dc_hdmi_out_en_fail;
		}
	}
	err = regulator_enable(of_hdmi_reg);
	if (err < 0) {
		pr_err("hdmi: couldn't enable regulator avdd_hdmi\n");
		goto dc_hdmi_out_en_fail;
	}
	if (!of_hdmi_pll) {
		of_hdmi_pll = regulator_get(dev, "avdd_hdmi_pll");
		if (IS_ERR_OR_NULL(of_hdmi_pll)) {
			pr_err("hdmi: couldn't get regulator avdd_hdmi_pll\n");
			of_hdmi_pll = NULL;
			regulator_put(of_hdmi_reg);
			of_hdmi_reg = NULL;
			err = PTR_ERR(of_hdmi_pll);
			goto dc_hdmi_out_en_fail;
		}
	}
	err = regulator_enable(of_hdmi_pll);
	if (err < 0) {
		pr_err("hdmi: couldn't enable regulator avdd_hdmi_pll\n");
		goto dc_hdmi_out_en_fail;
	}
dc_hdmi_out_en_fail:
	of_node_put(np_hdmi);
	return err;
}

static int dc_hdmi_out_disable(struct device *dev)
{
	struct platform_device *ndev = NULL;
	struct tegra_hdmi *hdmi = NULL;
	struct tegra_dc *dc = NULL;

	if (!dev)
		return -EINVAL;
	ndev = to_platform_device(dev);
	if (!ndev)
		return -EINVAL;

	dc = platform_get_drvdata(ndev);
	hdmi = tegra_dc_get_outdata(dc);

	/* Do not disable regulator when device is shutting down */
	if (hdmi->device_shutdown)
		return 0;

	if (of_hdmi_reg) {
		regulator_disable(of_hdmi_reg);
		regulator_put(of_hdmi_reg);
		of_hdmi_reg = NULL;
	}

	if (of_hdmi_pll) {
		regulator_disable(of_hdmi_pll);
		regulator_put(of_hdmi_pll);
		of_hdmi_pll = NULL;
	}

	return 0;
}

static int dc_hdmi_hotplug_init(struct device *dev)
{
	int err = 0;

	struct device_node *np_hdmi =
		of_find_node_by_path(HDMI_NODE);

	if (!np_hdmi || !of_device_is_available(np_hdmi)) {
		pr_info("%s: no valid hdmi node\n", __func__);
		goto dc_hdmi_hotplug_init_fail;
	}

	if (!of_hdmi_vddio) {
		of_hdmi_vddio = regulator_get(dev, "vdd_hdmi_5v0");
		if (IS_ERR_OR_NULL(of_hdmi_vddio)) {
			err = PTR_ERR(of_hdmi_vddio);
			pr_err("hdmi: couldn't get regulator vdd_hdmi_5v0\n");
			of_hdmi_vddio = NULL;
			goto dc_hdmi_hotplug_init_fail;

		}
	}
	err = regulator_enable(of_hdmi_vddio);
	if (err < 0) {
		pr_err("hdmi: couldn't enable regulator vdd_hdmi_5v0\n");
		regulator_put(of_hdmi_vddio);
		of_hdmi_vddio = NULL;
		goto dc_hdmi_hotplug_init_fail;
	}
dc_hdmi_hotplug_init_fail:
	of_node_put(np_hdmi);
	return err;
}

static int dc_hdmi_postsuspend(void)
{
	if (of_hdmi_vddio) {
		regulator_disable(of_hdmi_vddio);
		regulator_put(of_hdmi_vddio);
		of_hdmi_vddio = NULL;
	}
	return 0;
}

static void dc_hdmi_hotplug_report(bool state)
{
	static struct pinctrl_dev *pctl_dev = NULL;
	unsigned val = (state) ? TEGRA_PIN_PULL_DOWN : TEGRA_PIN_PULL_NONE;
	unsigned long conf;
	int ret;

	if (!pctl_dev)
		pctl_dev = pinctrl_get_dev_from_of_compatible(
			"nvidia,tegra124-pinmux");

	if (!pctl_dev) {
		pr_err("%s(): tegra pincontrol does not found\n", __func__);
		return;
	}

	conf = TEGRA_PINCONF_PACK(TEGRA_PINCONF_PARAM_PULL, val);

	ret = pinctrl_set_config_for_group_name(pctl_dev, "ddc_sda_pv5", conf);
	if (ret < 0)
		pr_err("%s(): ddc_sda_pv5 pinconfig failed: %d\n",
			__func__, ret);

	ret = pinctrl_set_config_for_group_name(pctl_dev, "ddc_scl_pv4", conf);
	if (ret < 0)
		pr_err("%s(): ddc_scl_pv4 pinconfig failed: %d\n",
			__func__, ret);
}

struct device_node *tegra_get_panel_node_out_type_check
	(struct tegra_dc *dc, u32 out_type)
{
	struct device_node *np_panel = NULL;
	struct device_node *np_def_out = NULL;
	u32 temp;

	if (dc->ndev->id == 0)
		np_panel = tegra_primary_panel_get_dt_node(NULL);
	else
		np_panel = tegra_secondary_panel_get_dt_node(NULL);
	if (!np_panel) {
		pr_err("There is no valid panel node\n");
		return NULL;
	}

	/*
	 * check if target panel node has expected output type
	 */
	np_def_out = of_get_child_by_name(np_panel,
		"disp-default-out");
	if (np_def_out)
		of_property_read_u32(np_def_out,
			"nvidia,out-type", &temp);
	if (temp == out_type)
		return np_panel;
	else {
		pr_err("target panel node has not proper out type\n");
		return NULL;
	}
}

struct tegra_dc_platform_data
		*of_dc_parse_platform_data(struct platform_device *ndev)
{
	struct tegra_dc_platform_data *pdata;
	struct device_node *np = ndev->dev.of_node;
	struct device_node *np_dsi = NULL;
	struct device_node *np_dsi_panel = NULL;
	struct device_node *np_sor = NULL;
	struct device_node *np_hdmi = NULL;
	struct device_node *np_dp_panel = NULL;
	struct device_node *timings_np = NULL;
	struct device_node *np_target_disp = NULL;
	struct device_node *sd_np = NULL;
	struct device_node *default_out_np = NULL;
	struct device_node *entry = NULL;
#ifdef CONFIG_TEGRA_DC_CMU
	struct device_node *cmu_np = NULL;
#endif
	int err;
	u32 temp;

	/*
	 * Memory for pdata, pdata->default_out, pdata->fb
	 * need to be allocated in default
	 * since it is expected data for these needs to be
	 * parsed from DTB.
	 */
	pdata = devm_kzalloc(&ndev->dev,
		sizeof(struct tegra_dc_platform_data), GFP_KERNEL);
	if (!pdata) {
		dev_err(&ndev->dev, "not enough memory\n");
		goto fail_parse;
	}

	pdata->default_out = devm_kzalloc(&ndev->dev,
		sizeof(struct tegra_dc_out), GFP_KERNEL);
	if (!pdata->default_out) {
		dev_err(&ndev->dev, "not enough memory\n");
		goto fail_parse;
	}

	pdata->fb = devm_kzalloc(&ndev->dev,
		sizeof(struct tegra_fb_data), GFP_KERNEL);
	if (!pdata->fb) {
		dev_err(&ndev->dev, "not enough memory\n");
		goto fail_parse;
	}

	/*
	 * determine dc out type,
	 * dc node defines nvidia,out-type to indicate
	 * what out type of display is used for
	 * current dc id.
	 */

	err = parse_dc_out_type(np, ndev, pdata->default_out);
	if (err) {
		pr_err("parse_dc_out_type err\n");
		goto fail_parse;
	}

	if (!of_property_read_u32(np, "nvidia,fb-bpp", &temp)) {
		pdata->fb->bits_per_pixel = (int)temp;
		OF_DC_LOG("fb bpp %d\n", pdata->fb->bits_per_pixel);
	} else {
		goto fail_parse;
	}

	if (!of_property_read_u32(np, "nvidia,fb-flags", &temp)) {
		if (temp == TEGRA_FB_FLIP_ON_PROBE)
			OF_DC_LOG("fb flip on probe\n");
		else if (temp == 0)
			OF_DC_LOG("do not flip fb on probe time\n");
		else {
			pr_err("invalid fb_flags\n");
			goto fail_parse;
		}
		pdata->fb->flags = (unsigned long)temp;
	}

	if (pdata->default_out->type == TEGRA_DC_OUT_DSI) {
		np_dsi = of_find_node_by_path(DSI_NODE);

		if (!np_dsi) {
			pr_err("%s: could not find dsi node\n", __func__);
			goto fail_parse;
		} else if (of_device_is_available(np_dsi)) {
			pdata->default_out->dsi = devm_kzalloc(&ndev->dev,
				sizeof(struct tegra_dsi_out), GFP_KERNEL);
			if (!pdata->default_out->dsi) {
				dev_err(&ndev->dev, "not enough memory\n");
				goto fail_parse;
			}
			np_dsi_panel = parse_dsi_settings(ndev, np_dsi,
				pdata);
			if (!np_dsi_panel)
				goto fail_parse;
			else
				np_target_disp = np_dsi_panel;
		}
	} else if (pdata->default_out->type == TEGRA_DC_OUT_DP ||
		pdata->default_out->type == TEGRA_DC_OUT_NVSR_DP ||
		   pdata->default_out->type == TEGRA_DC_OUT_FAKE_DP) {
		np_sor = (ndev->id) ? of_find_node_by_path(SOR1_NODE) :
			of_find_node_by_path(SOR_NODE);

		if (!np_sor) {
			pr_err("%s: could not find sor node\n", __func__);
			goto fail_parse;
		} else if (of_device_is_available(np_sor)) {
			pdata->default_out->dp_out = devm_kzalloc(&ndev->dev,
				sizeof(struct tegra_dp_out), GFP_KERNEL);
			if (!pdata->default_out->dp_out) {
				dev_err(&ndev->dev, "not enough memory\n");
				goto fail_parse;
			}
			np_dp_panel = parse_dp_settings(ndev, pdata);
			if (!np_dp_panel)
				goto fail_parse;
			else
				np_target_disp = np_dp_panel;
		}
	} else if (pdata->default_out->type == TEGRA_DC_OUT_HDMI) {
		bool hotplug_report = false;
		np_hdmi = of_find_node_by_path(HDMI_NODE);

		if (ndev->id == 0)
			np_target_disp
				= tegra_primary_panel_get_dt_node(pdata);
		else
			np_target_disp
				= tegra_secondary_panel_get_dt_node(pdata);

		if (!np_target_disp ||
			!of_device_is_available(np_target_disp)) {
			pr_err("/hdmi/hdmi-display node is NOT valid\n");
			goto fail_parse;
		}
		pdata->default_out->hdmi_out = devm_kzalloc(&ndev->dev,
				sizeof(struct tegra_hdmi_out), GFP_KERNEL);
		if (!pdata->default_out->hdmi_out) {
			dev_err(&ndev->dev, "not enough memory\n");
			goto fail_parse;
		}
		err = parse_tmds_config(ndev, np_target_disp,
				pdata->default_out);
		if (err)
			goto fail_parse;
		if (!of_property_read_u32(np_target_disp,
					"nvidia,hdmi-fpd-bridge", &temp)) {
			pdata->default_out->hdmi_out->
				hdmi2fpd_bridge_enable = (bool)temp;
			OF_DC_LOG("hdmi2fpd_bridge_enabled %d\n",
					pdata->default_out->hdmi_out->
					hdmi2fpd_bridge_enable);
		}
		/* fixed panel ops is dominant. If fixed panel ops
		 * is not defined, we set default hdmi panel ops */
		if (!pdata->default_out->enable &&
			!pdata->default_out->disable &&
			!pdata->default_out->postpoweron &&
			!pdata->default_out->prepoweroff &&
			!pdata->default_out->disable &&
			!pdata->default_out->hotplug_init &&
			!pdata->default_out->postsuspend &&
			!pdata->default_out->hotplug_report) {
			if (np_hdmi && of_device_is_available(np_hdmi)) {
				if (!of_property_read_u32(np_hdmi,
					"nvidia,hotplug-report", &temp)) {
					hotplug_report = (bool)temp;
				}
			}
			pdata->default_out->enable = dc_hdmi_out_enable;
			pdata->default_out->disable = dc_hdmi_out_disable;
			pdata->default_out->hotplug_init = dc_hdmi_hotplug_init;
			pdata->default_out->postsuspend = dc_hdmi_postsuspend;
			if (hotplug_report)
				pdata->default_out->hotplug_report =
				dc_hdmi_hotplug_report;
		}
	} else if (pdata->default_out->type == TEGRA_DC_OUT_LVDS) {
		np_sor = of_find_node_by_path(SOR_NODE);

		if (!np_sor) {
			pr_err("%s: could not find sor node\n", __func__);
			goto fail_parse;
		} else if (of_device_is_available(np_sor)) {
			np_target_disp = parse_lvds_settings(ndev, pdata);
			if (!np_target_disp)
				goto fail_parse;
		}
	} else
		pr_err("Failed to parse out type %d\n",
			pdata->default_out->type);

	default_out_np = of_get_child_by_name(np_target_disp,
		"disp-default-out");
	if (!default_out_np) {
		pr_err("%s: could not find disp-default-out node\n",
			__func__);
		goto fail_parse;
	} else {
		err = parse_disp_default_out(ndev, default_out_np,
			pdata->default_out, pdata->fb);
		if (err)
			goto fail_parse;
	}


	timings_np = of_get_child_by_name(np_target_disp,
		"display-timings");
	if (!timings_np) {
		if (pdata->default_out->type == TEGRA_DC_OUT_DSI) {
			pr_err("%s: could not find display-timings node\n",
				__func__);
			goto fail_parse;
		}
	} else if (pdata->default_out->type == TEGRA_DC_OUT_DSI ||
		   pdata->default_out->type == TEGRA_DC_OUT_FAKE_DP ||
			pdata->default_out->type == TEGRA_DC_OUT_LVDS) {
		/* pdata->default_out->type == TEGRA_DC_OUT_DSI or
		 * pdata->default_out->type == TEGRA_DC_OUT_LVDS
		 */
		pdata->default_out->n_modes =
			of_get_child_count(timings_np);
		if (pdata->default_out->n_modes == 0) {
			/*
			 * Should never happen !
			 */
			dev_err(&ndev->dev, "no timing given\n");
			goto fail_parse;
		}
		pdata->default_out->modes = devm_kzalloc(&ndev->dev,
			pdata->default_out->n_modes *
			sizeof(struct tegra_dc_mode), GFP_KERNEL);
		if (!pdata->default_out->modes) {
			dev_err(&ndev->dev, "not enough memory\n");
			goto fail_parse;
		}
	} else if (pdata->default_out->type == TEGRA_DC_OUT_HDMI) {
		/* pdata->default_out->type == TEGRA_DC_OUT_HDMI */
		pdata->default_out->n_modes =
			of_get_child_count(timings_np);
		if (pdata->default_out->n_modes) {
			pdata->default_out->modes = devm_kzalloc(&ndev->dev,
				pdata->default_out->n_modes *
				sizeof(struct tegra_dc_mode), GFP_KERNEL);
			if (!pdata->default_out->modes) {
				dev_err(&ndev->dev, "not enough memory\n");
				goto fail_parse;
			}
		} else {
#ifdef CONFIG_FRAMEBUFFER_CONSOLE
			/*
			 * Should never happen !
			 */
			dev_err(&ndev->dev, "no timing provided\n");
			goto fail_parse;
#endif
		}
	}

	sd_np = of_get_child_by_name(np_target_disp,
		"smartdimmer");
	if (!sd_np) {
		pr_info("%s: could not find SD settings node\n",
			__func__);
	} else {
		pdata->default_out->sd_settings =
			devm_kzalloc(&ndev->dev,
			sizeof(struct tegra_dc_sd_settings),
			GFP_KERNEL);
		if (!pdata->default_out->sd_settings) {
			dev_err(&ndev->dev, "not enough memory\n");
			goto fail_parse;
		}
	}

#ifdef CONFIG_TEGRA_DC_CMU
	cmu_np = of_get_child_by_name(np_target_disp,
		"cmu");

	if (!cmu_np) {
		pr_info("%s: could not find cmu node\n",
			__func__);
	} else {
		pdata->cmu = devm_kzalloc(&ndev->dev,
			sizeof(struct tegra_dc_cmu), GFP_KERNEL);
		if (!pdata->cmu) {
			dev_err(&ndev->dev, "not enough memory\n");
			goto fail_parse;
		}
	}
#endif

	/*
	 * parse sd_settings values
	 */
	if (pdata->default_out->sd_settings != NULL) {
		err = parse_sd_settings(sd_np, pdata->default_out->sd_settings);
		if (err)
			goto fail_parse;
	}

	if (pdata->default_out->modes != NULL) {
		struct tegra_dc_mode *cur_mode
			= pdata->default_out->modes;
		for_each_child_of_node(timings_np, entry) {
			err = parse_modes(pdata->default_out, entry, cur_mode);
			if (err)
				goto fail_parse;
			cur_mode++;
		}
	}

#ifdef CONFIG_TEGRA_DC_CMU
	if (pdata->cmu != NULL) {
		err = parse_cmu_data(cmu_np, pdata->cmu);
		if (err)
			goto fail_parse;
	}
#endif

	if (!of_property_read_u32(np, "nvidia,dc-flags", &temp)) {
		if ((temp != TEGRA_DC_FLAG_ENABLED) &&
			(temp != 0)) {
			pr_err("%s: invalid dc platform data flag\n",
				__func__);
			goto fail_parse;
		}
		pdata->flags = (unsigned long)temp;
		OF_DC_LOG("dc flag %lu\n", pdata->flags);
	}

	if (!of_property_read_u32(np, "nvidia,dc-ctrlnum", &temp)) {
		pdata->ctrl_num = (unsigned long)temp;
		OF_DC_LOG("dc controller index %lu\n", pdata->ctrl_num);
	}

	if (!of_property_read_u32(np, "nvidia,fb-win", &temp)) {
		pdata->fb->win = (int)temp;
		OF_DC_LOG("fb window Index %d\n", pdata->fb->win);
	}

	if (!of_property_read_u32(np, "nvidia,emc-clk-rate", &temp)) {
		pdata->emc_clk_rate = (unsigned long)temp;
		OF_DC_LOG("emc clk rate %lu\n", pdata->emc_clk_rate);
	}

	if (!of_property_read_u32(np, "win-mask", &temp)) {
		pdata->win_mask = (u32)temp;
		OF_DC_LOG("win mask 0x%x\n", temp);
	}
#ifdef CONFIG_TEGRA_DC_CMU
	if (!of_property_read_u32(np, "nvidia,cmu-enable", &temp)) {
		pdata->cmu_enable = (bool)temp;
		OF_DC_LOG("cmu enable %d\n", pdata->cmu_enable);
	} else {
		pdata->cmu_enable = false;
	}
#endif

	dev_info(&ndev->dev, "DT parsed successfully\n");
	of_node_put(np_dsi);
	of_node_put(np_sor);
	of_node_put(np_hdmi);
	return pdata;

fail_parse:
	of_node_put(np_dsi);
	of_node_put(np_sor);
	of_node_put(np_hdmi);
	return NULL;
}
#else
struct tegra_dc_platform_data
		*of_dc_parse_platform_data(struct platform_device *ndev)
{
	return NULL;
}
#endif
